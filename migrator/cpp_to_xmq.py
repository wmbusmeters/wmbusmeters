#!/usr/bin/env python3
"""
Driver C++ to XMQ Converter
Transforms a WMBusMeters driver from C++ format to XMQ format.
Usage: ./cpp_to_xmq.py <cpp_file> [output_file]
"""

import re
import sys
from pathlib import Path

class CPPDriverParser:
    def __init__(self, cpp_file):
        self.cpp_file = cpp_file
        self.content = None
        self.copyright_year = None
        self.copyright_authors = []
        self.driver_name = None
        self.meter_type = None
        self.aliases = []
        self.default_fields = []
        self.mvts = []
        self.library_fields = []  # For addOptionalLibraryFields
        self.fields = []
        self.tests = []
        self.skip_reason = None  # Track why we're skipping conversion

    def parse(self):
        """Main parse function"""
        with open(self.cpp_file, 'r') as f:
            self.content = f.read()

        # Check if driver uses processContent (cannot be auto-converted)
        if self._uses_process_content():
            self.skip_reason = "contains processContent() override - requires manual conversion"
            return

        self._extract_driver_info()
        self._extract_library_fields()  # NEW: Extract library fields
        self._extract_fields()
        self._extract_tests()

    def _uses_process_content(self):
        """Check if driver overrides processContent method"""
        # Pattern: void Driver::processContent or void ClassName::processContent
        pattern = r'void\s+\w+::\s*processContent\s*\('
        return bool(re.search(pattern, self.content))

    def _extract_copyright(self):
        """Extract copyright information, handling multi-line formats"""
        copyright_info = {'years': '', 'authors': []}

        # Get first 500 characters to find copyright block
        snippet = self.content[:500]

        # Find all copyright lines (year + author)
        lines = snippet.split('\n')

        years_found = set()

        for i, line in enumerate(lines[:15]):  # Check first 15 lines
            # Pattern 1: "Copyright (C) YYYY Author Name (gpl...)"
            m = re.match(r'.*Copyright \(C\) (\d{4}(?:-\d{4})?)\s+(.*?)\s*\(gpl', line)
            if m:
                year = m.group(1)
                author = m.group(2).strip()
                years_found.add(year)
                if author and author not in copyright_info['authors']:
                    copyright_info['authors'].append(author)
                continue

            # Pattern 2: "YYYY Author Name (gpl...)" (indented continuation)
            m = re.match(r'\s+(\d{4}(?:-\d{4})?)\s+(.*?)\s*\(gpl', line)
            if m and i > 0:  # Only if not the first line
                year = m.group(1)
                author = m.group(2).strip()
                years_found.add(year)
                if author and author not in copyright_info['authors']:
                    copyright_info['authors'].append(author)

        if years_found:
            # Format years: if only one year, use it; if range, format as range
            years_list = sorted(years_found)
            if len(years_list) == 1:
                copyright_info['years'] = years_list[0]
            else:
                # Get min and max year
                min_year = min(int(y.split('-')[0]) for y in years_list)
                max_year = max(int(y.split('-')[1] if '-' in y else y) for y in years_list)
                copyright_info['years'] = f"{min_year}-{max_year}"

        return copyright_info

    def _extract_driver_info(self):
        """Extract driver name, type, defaults, and detect (MVT) info"""
        # Extract copyright year and authors
        copyright_info = self._extract_copyright()
        self.copyright_year = copyright_info['years'] if copyright_info['years'] else "2017-2022"
        self.copyright_authors = copyright_info['authors']

        # Driver name
        m = re.search(r'di\.setName\("([^"]+)"\)', self.content)
        if m:
            self.driver_name = m.group(1)

        # Meter type
        m = re.search(r'di\.setMeterType\(MeterType::(\w+)\)', self.content)
        if m:
            self.meter_type = m.group(1)

        # Default fields - handle multi-line string concatenation (C++ adjacent string literals)
        m = re.search(r'di\.setDefaultFields\(\s*((?:"[^"]*"\s*)+)\)', self.content, re.MULTILINE)
        if m:
            # Extract all quoted strings and concatenate them
            fields_part = m.group(1)
            # Find all quoted strings
            quoted_strings = re.findall(r'"([^"]*)"', fields_part)
            fields_str = ''.join(quoted_strings)
            # Normalize whitespace
            fields_str = re.sub(r'\s+', ' ', fields_str)
            # Normalize _date_time → _datetime in default_fields
            fields_str = fields_str.replace('_date_time', '_datetime')
            self.default_fields = [f.strip() for f in fields_str.split(',') if f.strip()]

        # Aliases - extract addNameAlias calls
        for m in re.finditer(r'di\.addNameAlias\s*\(\s*"([^"]+)"\s*\)', self.content):
            self.aliases.append(m.group(1))

        # MVTs - extract manufacturer, type, and version
        for m in re.finditer(r'di\.addMVT\(([^,]+),\s*([^,]+),\s*([^)]+)\)', self.content):
            mfg_code = m.group(1).strip()
            type_val = m.group(3).strip()
            ver_val = m.group(2).strip()

            # Remove MANUFACTURER_ prefix
            mfg = mfg_code.replace('MANUFACTURER_', '')

            # Clean hex values
            type_val = re.sub(r'0x', '', type_val, flags=re.IGNORECASE)
            ver_val = re.sub(r'0x', '', ver_val, flags=re.IGNORECASE)

            # Store as (mfg, type, ver) which is the parameter order
            self.mvts.append((mfg, type_val, ver_val))

    def _extract_library_fields(self):
        """Extract library fields from addOptionalLibraryFields calls

        Preserves grouping: each call generates a separate 'use' line
        with comma-separated fields. This matches the XMQ runtime behavior
        where each 'use' element is processed independently.
        """
        # Pattern: addOptionalLibraryFields("field1,field2,field3")
        pattern = r'addOptionalLibraryFields\s*\(\s*"([^"]+)"\s*\)'

        for match in re.finditer(pattern, self.content):
            fields_str = match.group(1)
            # Keep the comma-separated string as a single group
            # Don't split into individual fields - preserve the original grouping
            self.library_fields.append(fields_str)

    def _extract_fields(self):
        """Extract field definitions from addXxxField calls and for loops"""
        # Find the Driver constructor
        m = re.search(r'Driver::Driver\(.*?\).*?:\s*MeterCommonImplementation.*?\{', self.content, re.DOTALL)
        if not m:
            return

        # Get the position after the opening brace
        ctor_start = m.end()

        # Find the matching closing brace
        ctor_body_text = self.content[ctor_start:]
        closing_brace_pos = self._find_matching_brace(ctor_body_text, 0)

        if closing_brace_pos == -1:
            return

        ctor_body = ctor_body_text[:closing_brace_pos]

        # First, extract fields from for loops
        self._extract_looped_fields(ctor_body)

        # Then extract all non-looped add*Field calls
        pattern = r'add(?:Numeric|String)Field\w*\s*\(\s*"([^"]+)"'

        for match in re.finditer(pattern, ctor_body):
            field_name = match.group(1)
            start_pos = match.start()

            # Skip if this field is inside a for loop (already processed)
            if self._is_inside_for_loop(ctor_body, start_pos):
                continue

            # Get the full method call
            field_call = self._extract_method_call(ctor_body, start_pos)
            if field_call:
                field_info = self._parse_field_call(field_call, field_name)
                if field_info:
                    self.fields.append(field_info)

    def _extract_tests(self):
        """Extract test cases from comments"""
        # Find test comment lines starting with // Test:
        pattern = r'//\s*Test:\s+([^\n]+)'

        for match in re.finditer(pattern, self.content):
            test_desc = match.group(1).strip()
            test_start = match.start()

            # Extract everything up to the next comment or end of file
            test_block_match = re.search(
                r'//\s*Test:.*?(?://\s*Test:|\Z)',
                self.content[test_start:],
                re.DOTALL
            )

            if test_block_match:
                test_block = test_block_match.group(0)

                # Extract telegram
                # C++ format: telegram=|segment1|segment2| (pipe-separated segments)
                # XMQ format: segment1_segment2 (underscore-separated)
                telegram_match = re.search(r'//\s*telegram=\|(.+?)\s*$', test_block, re.MULTILINE)
                if telegram_match:
                    raw = telegram_match.group(1)
                    # Split by pipe, filter empty parts, join with underscore
                    segments = [s for s in raw.split('|') if s.strip()]
                    telegram = '_'.join(segments)
                else:
                    telegram = None

                # Extract JSON - find the opening brace
                json_match = re.search(r'//\s*(\{.*?\})\s*$', test_block, re.MULTILINE | re.DOTALL)
                json_str = json_match.group(1) if json_match else None

                # Extract fields line
                fields_match = re.search(r'//\s*\|([^\n]+)', test_block)
                fields = fields_match.group(1) if fields_match else None

                # Extract comment (between Test: ... and telegram)
                lines = test_block.split('\n')
                comment = ""
                for i, line in enumerate(lines):
                    if 'Test:' in line and i + 1 < len(lines):
                        next_line = lines[i + 1].strip()
                        if not next_line.startswith('//'):
                            break
                        next_line = next_line[2:].strip()
                        # Remove "Comment: " prefix if present
                        if next_line.startswith('Comment:'):
                            next_line = next_line[8:].strip()
                        if next_line and not next_line.startswith('telegram') and not next_line.startswith('{'):
                            comment = next_line
                        break

                if telegram and json_str:
                    self.tests.append({
                        'args': test_desc,
                        'comment': comment,
                        'telegram': telegram,
                        'json': json_str,
                        'fields': fields
                    })

    def _find_matching_brace(self, text, start_pos):
        """Find the closing brace matching an opening brace"""
        depth = 1
        pos = start_pos + 1

        while pos < len(text) and depth > 0:
            # Handle raw strings R"delim(...)"
            if pos < len(text) - 1 and text[pos:pos+2] == 'R"':
                paren_pos = text.find('(', pos)
                if paren_pos > 0:
                    delim = text[pos+2:paren_pos]
                    end_str = ')' + delim + '"'
                    end_pos = text.find(end_str, paren_pos)
                    if end_pos > 0:
                        pos = end_pos + len(end_str)
                        continue

            # Handle regular strings
            if text[pos] == '"':
                pos += 1
                while pos < len(text) and text[pos] != '"':
                    if text[pos] == '\\':
                        pos += 2
                    else:
                        pos += 1
                pos += 1
                continue

            # Handle braces
            if text[pos] == '{':
                depth += 1
            elif text[pos] == '}':
                depth -= 1

            pos += 1

        return pos - 1 if depth == 0 else -1

    def _is_inside_for_loop(self, text, pos):
        """Check if position is inside a for loop"""
        # Look backwards to see if we're inside a for loop
        for_matches = list(re.finditer(r'for\s*\(', text))
        for for_match in for_matches:
            for_start = for_match.start()
            if pos > for_start:
                # Find the closing brace of this for loop
                loop_body_start = text.find('{', for_match.end())
                if loop_body_start > 0:
                    loop_body_text = text[loop_body_start:]
                    closing_pos = self._find_matching_brace(loop_body_text, 0)
                    if closing_pos > 0 and pos < loop_body_start + closing_pos:
                        return True
        return False

    def _extract_looped_fields(self, ctor_body):
        """Extract fields from for loops"""
        # Find all for loops
        for_pattern = r'for\s*\(\s*int\s+(\w+)\s*=\s*(\d+)\s*;\s*\1\s*<\s*(\d+)\s*;\s*\+\+\1\s*\)'

        for for_match in re.finditer(for_pattern, ctor_body):
            loop_var = for_match.group(1)
            start_val = int(for_match.group(2))
            end_val = int(for_match.group(3))

            # Find the opening brace of the loop body
            loop_body_start = ctor_body.find('{', for_match.end())
            if loop_body_start == -1:
                continue

            # Find the closing brace
            loop_body_text = ctor_body[loop_body_start:]
            closing_pos = self._find_matching_brace(loop_body_text, 0)
            if closing_pos == -1:
                continue

            loop_body = loop_body_text[1:closing_pos]

            # Extract field calls from the loop body
            pattern = r'add(?:Numeric|String)Field\w*\s*\('

            for field_match in re.finditer(pattern, loop_body):
                start_pos = field_match.start()

                # Get the full method call
                field_call = self._extract_method_call(loop_body, start_pos)
                if not field_call:
                    continue

                # Extract field name and arguments from tostrprintf or literal
                name_pattern = r'add(?:Numeric|String)Field\w*\s*\(\s*(?:tostrprintf\(\s*"([^"]+)"\s*,\s*([^)]+)\)|"([^"]+)")'
                name_match = re.search(name_pattern, field_call)
                if not name_match:
                    continue

                # Get either: tostrprintf with format and args, or literal string
                format_str = name_match.group(1)  # format from tostrprintf
                format_args = name_match.group(2)  # arguments to tostrprintf
                literal_str = name_match.group(3)  # literal string

                # Generate expanded fields for each loop iteration
                for i in range(start_val, end_val):
                    if format_str:
                        # tostrprintf case
                        field_name = self._substitute_tostrprintf(format_str, format_args, loop_var, i)
                    else:
                        # literal string case
                        field_name = literal_str

                    # Create a version of the field call with substituted loop variables
                    substituted_call = self._substitute_in_field_call(field_call, loop_var, i)

                    field_info = self._parse_field_call(substituted_call, field_name)
                    if field_info:
                        self.fields.append(field_info)

    def _substitute_tostrprintf(self, format_str, format_args, loop_var, iteration):
        """Substitute tostrprintf with actual values"""
        # Parse the format arguments (e.g., "i+1" or "i")
        args_str = format_args.strip()

        # Evaluate the argument with the loop variable value
        # Simple cases: "i+1", "i", "32+i", etc.
        try:
            # Replace loop variable with iteration value
            computed_arg = args_str.replace(loop_var, str(iteration))
            # Try to evaluate if it's a simple expression
            result_val = eval(computed_arg)
        except:
            # If evaluation fails, just use the iteration value
            result_val = iteration

        # Now substitute %d in format_str with the computed value
        result = format_str.replace('%d', str(result_val))
        return result

    def _substitute_in_field_call(self, field_call, loop_var, iteration):
        """Substitute loop variable in field call"""
        # Replace StorageNr(32+i) with StorageNr(32+iteration)
        # Replace StorageNr(i) with StorageNr(iteration)

        result = field_call

        # Pattern: StorageNr(32+i) or similar
        storage_pattern = rf'StorageNr\(\s*(\d+)\s*\+\s*{loop_var}\s*\)'
        result = re.sub(storage_pattern, lambda m: f'StorageNr({int(m.group(1)) + iteration})', result)

        # Pattern: StorageNr(i)
        storage_simple = rf'StorageNr\(\s*{loop_var}\s*\)'
        result = re.sub(storage_simple, f'StorageNr({iteration})', result)

        # Similarly for other numeric patterns
        # TariffNr(i) etc
        result = re.sub(rf'TariffNr\(\s*{loop_var}\s*\)', f'TariffNr({iteration})', result)
        result = re.sub(rf'TariffNr\(\s*(\d+)\s*\+\s*{loop_var}\s*\)',
                       lambda m: f'TariffNr({int(m.group(1)) + iteration})', result)

        return result

    def _extract_method_call(self, text, start_pos):
        """Extract a complete method call"""
        # Find opening paren
        paren_start = text.find('(', start_pos)
        if paren_start == -1:
            return None

        # Find matching closing paren
        depth = 0
        pos = paren_start

        while pos < len(text):
            # Handle raw strings
            if pos < len(text) - 1 and text[pos:pos+2] == 'R"':
                paren_pos = text.find('(', pos)
                if paren_pos > 0:
                    delim = text[pos+2:paren_pos]
                    end_str = ')' + delim + '"'
                    end_pos = text.find(end_str, paren_pos)
                    if end_pos > 0:
                        pos = end_pos + len(end_str)
                        continue

            # Handle regular strings
            if text[pos] == '"':
                pos += 1
                while pos < len(text) and text[pos] != '"':
                    if text[pos] == '\\':
                        pos += 2
                    else:
                        pos += 1
                pos += 1
                continue

            # Handle braces (for lookup tables)
            if text[pos] == '{':
                brace_depth = 1
                pos += 1
                while pos < len(text) and brace_depth > 0:
                    if pos < len(text) - 1 and text[pos:pos+2] == 'R"':
                        paren_pos = text.find('(', pos)
                        if paren_pos > 0:
                            delim = text[pos+2:paren_pos]
                            end_str = ')' + delim + '"'
                            end_pos = text.find(end_str, paren_pos)
                            if end_pos > 0:
                                pos = end_pos + len(end_str)
                                continue
                    elif text[pos] == '"':
                        pos += 1
                        while pos < len(text) and text[pos] != '"':
                            if text[pos] == '\\':
                                pos += 2
                            else:
                                pos += 1
                        pos += 1
                        continue
                    elif text[pos] == '{':
                        brace_depth += 1
                    elif text[pos] == '}':
                        brace_depth -= 1
                    pos += 1
                pos -= 1

            # Track parentheses
            if text[pos] == '(':
                depth += 1
            elif text[pos] == ')':
                depth -= 1
                if depth == 0:
                    # Find semicolon
                    while pos < len(text) and text[pos] != ';':
                        pos += 1
                    if pos < len(text):
                        return text[start_pos:pos+1]
                    break

            pos += 1

        return None

    def _parse_field_call(self, field_call, field_name):
        """Parse individual field call"""
        strings = re.findall(r'"([^"]*)"', field_call)
        description = strings[1] if len(strings) > 1 else ''

        quantity = self._get_quantity(field_call)
        vif_info = self._get_vif_info(field_call)
        attributes = self._get_attributes(field_call)
        lookup = self._get_lookup(field_call)

        # Auto-infer ErrorFlags vif_range and lookup for status fields with INCLUDE_TPL_STATUS
        # These fields extract from TPL (transport protocol layer) status
        if not vif_info.get('vif_range') and not vif_info.get('difvifkey'):
            if attributes and ('STATUS' in attributes or 'INCLUDE_TPL_STATUS' in attributes):
                # Set default measurement type if not present
                if 'measurement_type' not in vif_info:
                    vif_info['measurement_type'] = 'Instantaneous'
                # Infer ErrorFlags as the vif_range
                vif_info['vif_range'] = 'ErrorFlags'

                # If no explicit lookup, generate a minimal one for TPL status
                if not lookup:
                    lookup = {
                        'name': 'ERROR_FLAGS',
                        'map_type': 'BitToString',
                        'mask_bits': '0xffffffff',
                        'default_message': 'OK',
                        'mapping': []
                    }

        vif_scaling = self._get_vif_scaling(field_call)
        dif_signedness = self._get_dif_signedness(field_call)
        display_unit = self._get_display_unit(field_call)
        scale = self._get_scale(field_call)
        formula = self._get_formula(field_call)

        # Normalize _date_time suffix to _datetime (XMQ convention; CHANGES file documents this)
        field_name = field_name.replace('_date_time', '_datetime')

        return {
            'name': field_name,
            'description': description,
            'quantity': quantity,
            'vif_info': vif_info,
            'is_calculated': 'Calculator' in field_call,
            'attributes': attributes,
            'lookup': lookup,
            'vif_scaling': vif_scaling,
            'dif_signedness': dif_signedness,
            'display_unit': display_unit,
            'scale': scale,
            'formula': formula
        }

    def _get_quantity(self, field_call):
        """Extract quantity enum - all 25 types from units.h"""
        quantities = {
            # Basic SI quantities
            'Quantity::Time': 'Time',
            'Quantity::Length': 'Length',
            'Quantity::Mass': 'Mass',
            'Quantity::Amperage': 'Amperage',
            'Quantity::Temperature': 'Temperature',
            'Quantity::AmountOfSubstance': 'AmountOfSubstance',
            'Quantity::LuminousIntensity': 'LuminousIntensity',

            # Energy & Power
            'Quantity::Energy': 'Energy',
            'Quantity::Reactive_Energy': 'Reactive_Energy',
            'Quantity::Apparent_Energy': 'Apparent_Energy',
            'Quantity::Power': 'Power',
            'Quantity::Reactive_Power': 'Reactive_Power',
            'Quantity::Apparent_Power': 'Apparent_Power',

            # Fluid & Flow
            'Quantity::Volume': 'Volume',
            'Quantity::Flow': 'Flow',

            # Electrical & Environmental
            'Quantity::Voltage': 'Voltage',
            'Quantity::Frequency': 'Frequency',
            'Quantity::Pressure': 'Pressure',

            # Special
            'Quantity::PointInTime': 'PointInTime',
            'Quantity::RelativeHumidity': 'RelativeHumidity',
            'Quantity::HCA': 'HCA',
            'Quantity::Text': 'Text',
            'Quantity::Angle': 'Angle',
            'Quantity::Dimensionless': 'Dimensionless',
        }

        for pattern, quantity in quantities.items():
            if pattern in field_call:
                return quantity
        return 'Text'

    def _get_vif_info(self, field_call):
        """Extract VIF matching criteria"""
        vif_info = {}

        # Extract MeasurementType - support all 6 types from dvparser.h
        measurement_types = [
            ('MeasurementType::Instantaneous', 'Instantaneous'),
            ('MeasurementType::Maximum', 'Maximum'),
            ('MeasurementType::Minimum', 'Minimum'),
            ('MeasurementType::AtError', 'AtError'),
            ('MeasurementType::Any', 'Any'),
            ('MeasurementType::Unknown', 'Unknown'),
        ]
        for pattern, mtype in measurement_types:
            if pattern in field_call:
                vif_info['measurement_type'] = mtype
                break

        # Check VIFRanges - order from longest patterns first to avoid partial matches
        # All 46 types from dvparser.h LINE_OF_VIF_RANGES
        vif_ranges = [
            # Temperature-related (longest patterns first to avoid partial matches)
            ('VIFRange::ExternalTemperature', 'ExternalTemperature'),
            ('VIFRange::FlowTemperature', 'FlowTemperature'),
            ('VIFRange::ReturnTemperature', 'ReturnTemperature'),
            ('VIFRange::TemperatureDifference', 'TemperatureDifference'),

            # Energy types
            ('VIFRange::AnyEnergyVIF', 'AnyEnergyVIF'),
            ('VIFRange::EnergyMWh', 'EnergyMWh'),
            ('VIFRange::EnergyMJ', 'EnergyMJ'),
            ('VIFRange::EnergyGJ', 'EnergyGJ'),
            ('VIFRange::EnergyWh', 'EnergyWh'),

            # Power types
            ('VIFRange::AnyPowerVIF', 'AnyPowerVIF'),
            ('VIFRange::PowerW', 'PowerW'),
            ('VIFRange::PowerJh', 'PowerJh'),

            # Flow & Volume
            ('VIFRange::VolumeFlow', 'VolumeFlow'),
            ('VIFRange::AnyVolumeVIF', 'AnyVolumeVIF'),
            ('VIFRange::Volume', 'Volume'),

            # Date & Time
            ('VIFRange::DateTime', 'DateTime'),
            ('VIFRange::Date', 'Date'),
            ('VIFRange::OnTime', 'OnTime'),
            ('VIFRange::OperatingTime', 'OperatingTime'),
            ('VIFRange::ActualityDuration', 'ActualityDuration'),
            ('VIFRange::DurationSinceReadout', 'DurationSinceReadout'),
            ('VIFRange::DurationOfTariff', 'DurationOfTariff'),

            # Environmental & Properties
            ('VIFRange::RelativeHumidity', 'RelativeHumidity'),
            ('VIFRange::Pressure', 'Pressure'),
            ('VIFRange::ErrorFlags', 'ErrorFlags'),
            ('VIFRange::HeatCostAllocation', 'HeatCostAllocation'),

            # Electrical
            ('VIFRange::Voltage', 'Voltage'),
            ('VIFRange::Amperage', 'Amperage'),

            # Identification & Info fields (info/text types)
            ('VIFRange::FirmwareVersion', 'FirmwareVersion'),
            ('VIFRange::SoftwareVersion', 'SoftwareVersion'),
            ('VIFRange::HardwareVersion', 'HardwareVersion'),
            ('VIFRange::EnhancedIdentification', 'EnhancedIdentification'),
            ('VIFRange::FabricationNo', 'FabricationNo'),
            ('VIFRange::Manufacturer', 'Manufacturer'),
            ('VIFRange::ModelVersion', 'ModelVersion'),
            ('VIFRange::ParameterSet', 'ParameterSet'),
            ('VIFRange::Location', 'Location'),
            ('VIFRange::Customer', 'Customer'),
            ('VIFRange::DigitalInput', 'DigitalInput'),
            ('VIFRange::DigitalOutput', 'DigitalOutput'),
            ('VIFRange::Medium', 'Medium'),
            ('VIFRange::AccessNumber', 'AccessNumber'),

            # Counter & special
            ('VIFRange::Dimensionless', 'Dimensionless'),
            ('VIFRange::CumulationCounter', 'CumulationCounter'),
            ('VIFRange::ResetCounter', 'ResetCounter'),
            ('VIFRange::SpecialSupplierInformation', 'SpecialSupplierInformation'),
            ('VIFRange::RemainingBattery', 'RemainingBattery'),
        ]

        for pattern, vif_name in vif_ranges:
            if pattern in field_call:
                vif_info['vif_range'] = vif_name
                break

        m = re.search(r'DifVifKey\("([^"]+)"\)', field_call)
        if m:
            vif_info['difvifkey'] = m.group(1)

        # Extract VIFCombinable values - complete list from LIST_OF_VIF_COMBINABLES in dvparser.h
        vif_combinables = {
            'VIFCombinable::Average': 'Average',
            'VIFCombinable::InverseCompactProfile': 'InverseCompactProfile',
            'VIFCombinable::RelativeDeviation': 'RelativeDeviation',
            'VIFCombinable::RecordErrorCodeMeterToController': 'RecordErrorCodeMeterToController',
            'VIFCombinable::StandardConformantDataContent': 'StandardConformantDataContent',
            'VIFCombinable::CompactProfileWithRegister': 'CompactProfileWithRegister',
            'VIFCombinable::CompactProfile': 'CompactProfile',
            'VIFCombinable::PerSecond': 'PerSecond',
            'VIFCombinable::PerMinute': 'PerMinute',
            'VIFCombinable::PerHour': 'PerHour',
            'VIFCombinable::PerDay': 'PerDay',
            'VIFCombinable::PerWeek': 'PerWeek',
            'VIFCombinable::PerMonth': 'PerMonth',
            'VIFCombinable::PerYear': 'PerYear',
            'VIFCombinable::PerRevolutionMeasurement': 'PerRevolutionMeasurement',
            'VIFCombinable::IncrPerInputPulseChannel0': 'IncrPerInputPulseChannel0',
            'VIFCombinable::IncrPerInputPulseChannel1': 'IncrPerInputPulseChannel1',
            'VIFCombinable::IncrPerOutputPulseChannel0': 'IncrPerOutputPulseChannel0',
            'VIFCombinable::IncrPerOutputPulseChannel1': 'IncrPerOutputPulseChannel1',
            'VIFCombinable::PerLitre': 'PerLitre',
            'VIFCombinable::PerM3': 'PerM3',
            'VIFCombinable::PerKg': 'PerKg',
            'VIFCombinable::PerKelvin': 'PerKelvin',
            'VIFCombinable::PerKWh': 'PerKWh',
            'VIFCombinable::PerGJ': 'PerGJ',
            'VIFCombinable::PerKW': 'PerKW',
            'VIFCombinable::PerKelvinLitreW': 'PerKelvinLitreW',
            'VIFCombinable::PerVolt': 'PerVolt',
            'VIFCombinable::PerAmpere': 'PerAmpere',
            'VIFCombinable::MultipliedByS': 'MultipliedByS',
            'VIFCombinable::MultipliedBySDivV': 'MultipliedBySDivV',
            'VIFCombinable::MultipliedBySDivA': 'MultipliedBySDivA',
            'VIFCombinable::StartDateTimeOfAB': 'StartDateTimeOfAB',
            'VIFCombinable::UncorrectedMeterUnit': 'UncorrectedMeterUnit',
            'VIFCombinable::ForwardFlow': 'ForwardFlow',
            'VIFCombinable::BackwardFlow': 'BackwardFlow',
            'VIFCombinable::ValueAtBaseCondC': 'ValueAtBaseCondC',
            'VIFCombinable::ObisDeclaration': 'ObisDeclaration',
            'VIFCombinable::LowerLimit': 'LowerLimit',
            'VIFCombinable::ExceedsLowerLimit': 'ExceedsLowerLimit',
            'VIFCombinable::DateTimeExceedsLowerFirstBegin': 'DateTimeExceedsLowerFirstBegin',
            'VIFCombinable::DateTimeExceedsLowerFirstEnd': 'DateTimeExceedsLowerFirstEnd',
            'VIFCombinable::DateTimeExceedsLowerLastBegin': 'DateTimeExceedsLowerLastBegin',
            'VIFCombinable::DateTimeExceedsLowerLastEnd': 'DateTimeExceedsLowerLastEnd',
            'VIFCombinable::UpperLimit': 'UpperLimit',
            'VIFCombinable::ExceedsUpperLimit': 'ExceedsUpperLimit',
            'VIFCombinable::DateTimeExceedsUpperFirstBegin': 'DateTimeExceedsUpperFirstBegin',
            'VIFCombinable::DateTimeExceedsUpperFirstEnd': 'DateTimeExceedsUpperFirstEnd',
            'VIFCombinable::DateTimeExceedsUpperLastBegin': 'DateTimeExceedsUpperLastBegin',
            'VIFCombinable::DateTimeExceedsUpperLastEnd': 'DateTimeExceedsUpperLastEnd',
            'VIFCombinable::DurationExceedsLowerFirst': 'DurationExceedsLowerFirst',
            'VIFCombinable::DurationExceedsLowerLast': 'DurationExceedsLowerLast',
            'VIFCombinable::DurationExceedsUpperFirst': 'DurationExceedsUpperFirst',
            'VIFCombinable::DurationExceedsUpperLast': 'DurationExceedsUpperLast',
            'VIFCombinable::DurationOfDFirst': 'DurationOfDFirst',
            'VIFCombinable::DurationOfDLast': 'DurationOfDLast',
            'VIFCombinable::ValueDuringLowerLimitExceeded': 'ValueDuringLowerLimitExceeded',
            'VIFCombinable::LeakageValues': 'LeakageValues',
            'VIFCombinable::OverflowValues': 'OverflowValues',
            'VIFCombinable::ValueDuringUpperLimitExceeded': 'ValueDuringUpperLimitExceeded',
            'VIFCombinable::DateTimeOfDEFirstBegin': 'DateTimeOfDEFirstBegin',
            'VIFCombinable::DateTimeOfDEFirstEnd': 'DateTimeOfDEFirstEnd',
            'VIFCombinable::DateTimeOfDELastBegin': 'DateTimeOfDELastBegin',
            'VIFCombinable::DateTimeOfDELastEnd': 'DateTimeOfDELastEnd',
            'VIFCombinable::MultiplicativeCorrectionFactorForValue103': 'MultiplicativeCorrectionFactorForValue103',
            'VIFCombinable::MultiplicativeCorrectionFactorForValue': 'MultiplicativeCorrectionFactorForValue',
            'VIFCombinable::AdditiveCorrectionConstant': 'AdditiveCorrectionConstant',
            'VIFCombinable::CombinableVIFExtension': 'CombinableVIFExtension',
            'VIFCombinable::FutureValue': 'FutureValue',
            'VIFCombinable::MfctSpecific': 'MfctSpecific',
            'VIFCombinable::AtPhase1': 'AtPhase1',
            'VIFCombinable::AtPhase2': 'AtPhase2',
            'VIFCombinable::AtPhase3': 'AtPhase3',
            'VIFCombinable::AtNeutral': 'AtNeutral',
            'VIFCombinable::BetweenPhaseL1AndL2': 'BetweenPhaseL1AndL2',
            'VIFCombinable::BetweenPhaseL2AndL3': 'BetweenPhaseL2AndL3',
            'VIFCombinable::BetweenPhaseL3AndL1': 'BetweenPhaseL3AndL1',
            'VIFCombinable::AtQuadrantQ1': 'AtQuadrantQ1',
            'VIFCombinable::AtQuadrantQ2': 'AtQuadrantQ2',
            'VIFCombinable::AtQuadrantQ3': 'AtQuadrantQ3',
            'VIFCombinable::AtQuadrantQ4': 'AtQuadrantQ4',
            'VIFCombinable::DeltaBetweenImportAndExport': 'DeltaBetweenImportAndExport',
            'VIFCombinable::AccumulationOfAbsoluteValue': 'AccumulationOfAbsoluteValue',
            'VIFCombinable::DataPresentedWithTypeC': 'DataPresentedWithTypeC',
            'VIFCombinable::DataPresentedWithTypeD': 'DataPresentedWithTypeD',
        }
        found_combinables = []
        for pattern, combinable in vif_combinables.items():
            if pattern in field_call:
                found_combinables.append(combinable)
        if found_combinables:
            vif_info['add_combinable'] = found_combinables

        # Extract VIFCombinableRaw values → add_combinable_raw in XMQ
        raw_combinables = re.findall(r'VIFCombinableRaw\s*\(\s*(0[xX][0-9a-fA-F]+|\d+)\s*\)', field_call)
        if raw_combinables:
            vif_info['add_combinable_raw'] = raw_combinables

        # Extract StorageNr — handles both single StorageNr(n) and range StorageNr(a),StorageNr(b)
        storage_matches = re.findall(r'StorageNr\((\d+)\)', field_call)
        if len(storage_matches) == 2:
            vif_info['storage_nr'] = f"{storage_matches[0]},{storage_matches[1]}"
        elif len(storage_matches) == 1:
            vif_info['storage_nr'] = storage_matches[0]

        # Extract TariffNr — handles both single TariffNr(n) and range TariffNr(a),TariffNr(b)
        tariff_matches = re.findall(r'TariffNr\((\d+)\)', field_call)
        if len(tariff_matches) == 2:
            vif_info['tariff_nr'] = f"{tariff_matches[0]},{tariff_matches[1]}"
        elif len(tariff_matches) == 1:
            vif_info['tariff_nr'] = tariff_matches[0]

        # Extract SubUnitNr
        m = re.search(r'SubUnitNr\((\d+)\)', field_call)
        if m:
            vif_info['subunit_nr'] = m.group(1)

        return vif_info

    def _get_attributes(self, field_call):
        """Extract field attributes - all 7 PrintProperty types from meters.h"""
        attributes = []

        # All PrintProperty types: REQUIRED, DEPRECATED, STATUS, INCLUDE_TPL_STATUS, INJECT_INTO_STATUS, HIDE, Unknown
        attribute_map = {
            'PrintProperty::REQUIRED': 'REQUIRED',
            'PrintProperty::DEPRECATED': 'DEPRECATED',
            'PrintProperty::STATUS': 'STATUS',
            'PrintProperty::INCLUDE_TPL_STATUS': 'INCLUDE_TPL_STATUS',
            'PrintProperty::INJECT_INTO_STATUS': 'INJECT_INTO_STATUS',
            'PrintProperty::HIDE': 'HIDE',
            'PrintProperty::Unknown': 'Unknown',
        }

        for prop, attr in attribute_map.items():
            if prop in field_call:
                attributes.append(attr)

        return attributes if attributes else None

    def _get_lookup(self, field_call):
        """Extract lookup table information"""
        # Check if this field has a lookup by looking for typical lookup patterns
        # Look for MaskBits or AutoMask which are key indicators
        if 'MaskBits' not in field_call and 'AutoMask' not in field_call:
            return None

        lookup = {}

        # Extract lookup name (will be a quoted string like "ERROR_FLAGS")
        # It typically comes after MapType, handle AutoMask case
        name_match = re.search(r'Translate::MapType::\w+\s*,\s*(AlwaysTrigger|Never|TriggerBits\([^)]*\)),?\s*(AutoMask|MaskBits\s*\(\s*(?:\d+|0x[0-9A-Fa-f]+|\w+)\s*\))\s*,\s*"([^"]+)"', field_call)

        if name_match:
            lookup['default_message'] = name_match.group(3)
            # The actual name should be before this, look for it
            before_maptype = field_call[:field_call.find('Translate::MapType')]
            name_search = re.search(r'"([A-Z_0-9]+)"', before_maptype[-100:])
            if name_search:
                lookup['name'] = name_search.group(1)
        else:
            # Try simpler extraction
            name_match = re.search(r'"([A-Z_0-9]+)"\s*,\s*Translate::MapType', field_call)
            if name_match:
                lookup['name'] = name_match.group(1)

        # Extract map type
        if 'Translate::MapType::DecimalsToString' in field_call:
            lookup['map_type'] = 'DecimalsToString'
        elif 'Translate::MapType::BitToString' in field_call:
            lookup['map_type'] = 'BitToString'
        elif 'Translate::MapType::IndexToString' in field_call:
            lookup['map_type'] = 'IndexToString'
        elif 'Translate::MapType::' in field_call:
            type_match = re.search(r'Translate::MapType::(\w+)', field_call)
            if type_match:
                lookup['map_type'] = type_match.group(1)

        # Extract MaskBits value
        mask_match = re.search(r'MaskBits\((\d+|0x[0-9A-Fa-f]+|\w+)\)', field_call)
        if mask_match:
            value = mask_match.group(1)
            # Convert to hex if it's a decimal number, keep hex format for hex values
            try:
                if value.startswith('0x'):
                    # Already hex, keep it as is
                    lookup['mask_bits'] = value
                else:
                    # Convert decimal to hex for consistent formatting
                    decimal_val = int(value)
                    lookup['mask_bits'] = f'0x{decimal_val:x}'
            except:
                lookup['mask_bits'] = value
        elif 'AutoMask' in field_call:
            # For AutoMask, use a standard full mask value
            lookup['mask_bits'] = '0xffffffff'

        # Extract default message - look specifically for DefaultMessage("...") call first
        default_msg_match = re.search(r'DefaultMessage\s*\(\s*"([^"]*)"\s*\)', field_call)
        if default_msg_match:
            lookup['default_message'] = default_msg_match.group(1)
        elif mask_match or 'AutoMask' in field_call:
            # Fallback: look for a quoted string after MaskBits/AutoMask that isn't inside a Translate::Map
            if mask_match:
                search_start = mask_match.end()
            else:
                automask_pos = field_call.find('AutoMask')
                search_start = automask_pos + len('AutoMask') if automask_pos >= 0 else 0

            after_mask = field_call[search_start:]
            # Only treat it as default_message if it's NOT immediately followed by a comma then value
            fallback_match = re.search(r'[,\s]"([^"]+)"(?!\s*,\s*(?:TestBit|Translate))', after_mask)
            if fallback_match:
                lookup['default_message'] = fallback_match.group(1)

        # Extract ALL value/name mappings from the entire field_call
        # Format 1: { value, "name" } (older style)
        # Format 2: Translate::Map(value, "name", TestBit::...) (newer style)
        mappings = []
        seen_values = set()
        for pattern in [
            r'\{\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*"([^"]+)"\s*\}',
            r'Translate::Map\s*\(\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*"([^"]+)"',
        ]:
            for match in re.finditer(pattern, field_call):
                value = match.group(1)
                name = match.group(2)

                # Standardize value to lowercase hex with proper 0x prefix
                if not value.startswith('0x'):
                    try:
                        # Convert decimal to hex
                        value = f"0x{int(value):x}"
                    except:
                        pass  # Keep original if conversion fails
                else:
                    # Normalize hex to lowercase
                    value = '0x' + value[2:].lower()

                if value not in seen_values:
                    seen_values.add(value)
                    mappings.append({'value': value, 'name': name})

        if mappings:
            lookup['mapping'] = mappings

        return lookup if 'name' in lookup and len(lookup) > 1 else None

    def _get_vif_scaling(self, field_call):
        """Extract VifScaling parameter"""
        m = re.search(r'VifScaling::(\w+)', field_call)
        return m.group(1) if m else None

    def _get_dif_signedness(self, field_call):
        """Extract DifSignedness parameter"""
        m = re.search(r'DifSignedness::(\w+)', field_call)
        return m.group(1) if m else None

    def _get_display_unit(self, field_call):
        """Extract display_unit from Unit parameter"""
        # Unit::DateLT -> date
        unit_map = {
            'Unit::DateLT': 'date',
            'Unit::TimeLT': 'time',
            'Unit::DateTimeLT': 'datetime',
        }
        for pattern, unit in unit_map.items():
            if pattern in field_call:
                return unit

        # Also check for VIFRange::DateTime which maps to datetime display unit
        if 'VIFRange::DateTime' in field_call:
            return 'datetime'

        return None

    def _get_scale(self, field_call):
        """Extract scale parameter (default is 1.0)"""
        # Look for ),\s*scale parameter or ); // with scale
        m = re.search(r',\s*(\d+\.?\d*)\s*\)', field_call)
        if m:
            scale_val = m.group(1)
            if scale_val != '1' and scale_val != '1.0':
                return scale_val
        return None

    def _get_formula(self, field_call):
        """Extract formula from calculated fields"""
        if 'addNumericFieldWithCalculator' not in field_call:
            return None

        # Extract the formula from the method call
        # The formula is typically after the quantity parameter
        m = re.search(r'Quantity::\w+\s*,\s*([R]?"[^"]*"(?:\s*[R]?"[^"]*")*)', field_call)
        if not m:
            return None

        formula_raw = m.group(1).strip()

        # Remove R" prefix and quotes for raw strings
        formula = re.sub(r'[R]?"([^"]*)"', r'\1', formula_raw)
        # Clean up the formula by removing extra whitespace but preserving structure
        formula = re.sub(r'\s+', ' ', formula).strip()

        # Check if it looks like a formula (contains operators or function calls)
        if any(op in formula for op in ['(', ')', '+', '-', '*', '/', 'sqrt', 'counter', 'log']):
            return formula

        return None

    def to_xmq(self):
        """Generate XMQ format"""
        lines = []
        # Build copyright with all authors
        if self.copyright_authors:
            authors_str = ', '.join(self.copyright_authors)
            copyright = f"// Copyright (C) {self.copyright_year} {authors_str} (gpl-3.0-or-later)"
        else:
            copyright = f"// Copyright (C) {self.copyright_year} Fredrik Öhrström (gpl-3.0-or-later)" if self.copyright_year else "// Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)"
        lines.append(copyright)
        lines.append("driver {")
        lines.append(f"    name           = {self.driver_name}")
        if self.aliases:
            lines.append(f"    aliases        = {','.join(self.aliases)}")
        lines.append(f"    meter_type     = {self.meter_type}")
        if self.default_fields:
            lines.append(f"    default_fields = {','.join(self.default_fields)}")

        if self.mvts:
            lines.append("    detect {")
            for mfg, version, product in self.mvts:
                lines.append(f"        mvt = {mfg},{version},{product}")
            lines.append("    }")

        if self.library_fields:
            lines.append("    library {")
            for field in self.library_fields:
                lines.append(f"        use = {field}")
            lines.append("    }")

        if self.fields:
            lines.append("    fields {")
            for field in self.fields:
                lines.append("        field {")
                # Quote field names containing special characters like {tariff_counter}
                fname = field['name']
                if '{' in fname or '}' in fname:
                    lines.append(f"            name     = '{fname}'")
                else:
                    lines.append(f"            name     = {fname}")
                # Output quantity, fallback to Text if not set
                quantity = field['quantity'] or 'Text'
                lines.append(f"            quantity = {quantity}")
                if field['description']:
                    desc = field['description']
                    # XMQ supports both ' and " as quote chars; use " when content contains '
                    if "'" in desc:
                        lines.append(f'            info     = "{desc}"')
                    else:
                        lines.append(f"            info     = '{desc}'")
                # Output vif_scaling if present (important for data interpretation)
                if field.get('vif_scaling'):
                    lines.append(f"            vif_scaling    = {field['vif_scaling']}")
                # Output dif_signedness if present (important for data interpretation)
                if field.get('dif_signedness'):
                    lines.append(f"            dif_signedness = {field['dif_signedness']}")
                if field.get('display_unit'):
                    lines.append(f"            display_unit = {field['display_unit']}")
                if field.get('scale'):
                    lines.append(f"            scale = {field['scale']}")
                if field.get('formula'):
                    lines.append(f"            formula = '{field['formula']}'")
                if field.get('attributes'):
                    lines.append(f"            attributes = {','.join(field['attributes'])}")
                if field['vif_info']:
                    vif = field['vif_info']
                    # Only output match block if it's complete:
                    # Either has difvifkey OR has vif_range (measurement_type defaults to Instantaneous if not present)
                    has_difvifkey = 'difvifkey' in vif
                    has_vif_range = 'vif_range' in vif
                    has_measurement_type = 'measurement_type' in vif

                    # Validate match block completeness:
                    # - difvifkey alone is valid, OR
                    # - vif_range alone is valid (measurement_type defaults to Instantaneous), OR
                    # - both vif_range and measurement_type
                    is_valid = has_difvifkey or has_vif_range

                    if is_valid:
                        lines.append("            match {")
                        if not has_difvifkey:
                            # difvifkey excludes measurement_type/vif_range
                            measurement_type = vif.get('measurement_type', 'Instantaneous')
                            lines.append(f"                measurement_type = {measurement_type}")
                        if has_vif_range:
                            # Override vif_range for calculated date fields
                            vif_range = vif['vif_range']
                            if field.get('quantity') == 'PointInTime' and field.get('formula'):
                                # For calculated date fields with formulas, use Date vif_range
                                vif_range = 'Date'
                            lines.append(f"                vif_range        = {vif_range}")
                        if has_difvifkey:
                            lines.append(f"                difvifkey        = {vif['difvifkey']}")
                        if 'storage_nr' in vif:
                            lines.append(f"                storage_nr       = {vif['storage_nr']}")
                        if 'tariff_nr' in vif:
                            lines.append(f"                tariff_nr        = {vif['tariff_nr']}")
                        if 'subunit_nr' in vif:
                            lines.append(f"                subunit_nr       = {vif['subunit_nr']}")
                        if 'add_combinable' in vif:
                            for comb in vif['add_combinable']:
                                lines.append(f"                add_combinable   = {comb}")
                        if 'add_combinable_raw' in vif:
                            for raw in vif['add_combinable_raw']:
                                lines.append(f"                add_combinable_raw = {raw}")
                        lines.append("            }")
                if field.get('lookup'):
                    lookup = field['lookup']
                    lines.append("            lookup {")
                    if 'name' in lookup:
                        lines.append(f"                name            = {lookup['name']}")
                    if 'map_type' in lookup:
                        lines.append(f"                map_type        = {lookup['map_type']}")
                    if 'mask_bits' in lookup:
                        lines.append(f"                mask_bits       = {lookup['mask_bits']}")
                    if 'default_message' in lookup:
                        lines.append(f"                default_message = {lookup['default_message']}")
                    if 'mapping' in lookup:
                        for bit_map in lookup['mapping']:
                            lines.append("                map {")
                            lines.append(f"                    name  = {bit_map['name']}")
                            lines.append(f"                    value = {bit_map['value']}")
                            lines.append(f"                    test  = Set")
                            lines.append("                }")
                    lines.append("            }")
                lines.append("        }")
            lines.append("    }")

        if self.tests:
            lines.append("    tests {")
            for test in self.tests:
                lines.append("        test {")
                lines.append(f"            args     = '{test['args']}'")
                if test['comment']:
                    lines.append(f"            comment  = '{test['comment']}'")
                lines.append(f"            telegram = {test['telegram']}")
                # Normalize _date_time JSON keys and escape quotes
                json_str = test['json'].replace('_date_time"', '_datetime"')
                json_escaped = json_str.replace("'", "\\'")
                lines.append(f"            json     = '{json_escaped}'")
                if test['fields']:
                    lines.append(f"            fields   = '{test['fields']}'")
                lines.append("        }")
            lines.append("    }")

        lines.append("}")
        return '\n'.join(lines)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <cpp_file> [output_file]")
        print("  Converts a C++ driver to XMQ format")
        sys.exit(1)

    cpp_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else None

    if not Path(cpp_file).exists():
        print(f"Error: File '{cpp_file}' not found", file=sys.stderr)
        sys.exit(1)

    parser = CPPDriverParser(cpp_file)
    parser.parse()

    # Check if conversion was skipped
    if parser.skip_reason:
        print(f"Skipping {Path(cpp_file).name}: {parser.skip_reason}", file=sys.stderr)
        sys.exit(2)

    xmq = parser.to_xmq()

    if output_file:
        Path(output_file).write_text(xmq)
        print(f"✓ Generated: {output_file}")
    else:
        print(xmq)


if __name__ == '__main__':
    main()

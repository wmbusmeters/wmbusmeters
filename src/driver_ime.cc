#include "Meter.h"
#include <sstream>
#include <iomanip>

class MeterIME : public Meter {
public:
    MeterIME() {
        // Identification code should match the specifics of your meter
        di.addDetection(MANUFACTURER_IME, 0x08, 0x55);
    }

    void processContent(const std::vector<uint8_t> &data, const struct wmbusframe &frame) override {
        if (data.size() < 11) return;  // Check that the data block is adequately long

        // Typically, the secondary address would be decoded here
        std::string secondaryAddress = decodeSecondaryAddress(data);
        std::cout << "Secondary Address: " << secondaryAddress << std::endl;

        // Iterate through data blocks
        for (size_t i = 0; i < data.size(); i += 11) {
            std::vector<uint8_t> field(data.begin() + i, data.begin() + i + 11);

            // Handling all data types from the table for telegrams 1 to 4
            // Telegram 1
            if (fieldMatch(field, {0x84, 0x90, 0x10, 0xFF, 0x80, 0x84, 0x3B})) {
                addValue(field, 0.01, "Positive Three-phase Active Energy (Total)", "kWh");
            } else if (fieldMatch(field, {0x84, 0x90, 0x10, 0xFF, 0x80, 0x84, 0x3C})) {
                addValue(field, 0.01, "Negative Three-phase Active Energy (Total)", "kWh");
            } else if (fieldMatch(field, {0x84, 0x90, 0x10, 0xFF, 0x81, 0x84, 0x3B})) {
                addValue(field, 0.01, "Positive Three-phase Reactive Energy (Total)", "kvarh");
            } else if (fieldMatch(field, {0x84, 0x90, 0x10, 0xFF, 0x81, 0x84, 0x3C})) {
                addValue(field, 0.01, "Negative Three-phase Reactive Energy (Total)", "kvarh");
            } else if (fieldMatch(field, {0x84, 0x10, 0xFF, 0x80, 0x84, 0x3B})) {
                addValue(field, 0.01, "Positive Three-phase Active Energy (Tariff 1)", "kWh");
            } else if (fieldMatch(field, {0x84, 0x20, 0xFF, 0x80, 0x84, 0x3B})) {
                addValue(field, 0.01, "Positive Three-phase Active Energy (Tariff 2)", "kWh");
            } else if (fieldMatch(field, {0x84, 0x10, 0xFF, 0x80, 0x84, 0x3C})) {
                addValue(field, 0.01, "Negative Three-phase Active Energy (Tariff 1)", "kWh");
            } else if (fieldMatch(field, {0x84, 0x20, 0xFF, 0x80, 0x84, 0x3C})) {
                addValue(field, 0.01, "Negative Three-phase Active Energy (Tariff 2)", "kWh");
            } else if (fieldMatch(field, {0x84, 0x10, 0xFF, 0x81, 0x84, 0x3B})) {
                addValue(field, 0.01, "Positive Three-phase Reactive Energy (Tariff 1)", "kvarh");
            } else if (fieldMatch(field, {0x84, 0x20, 0xFF, 0x81, 0x84, 0x3B})) {
                addValue(field, 0.01, "Positive Three-phase Reactive Energy (Tariff 2)", "kvarh");
            } else if (fieldMatch(field, {0x84, 0x10, 0xFF, 0x81, 0x84, 0x3C})) {
                addValue(field, 0.01, "Negative Three-phase Reactive Energy (Tariff 1)", "kvarh");
            } else if (fieldMatch(field, {0x84, 0x20, 0xFF, 0x81, 0x84, 0x3C})) {
                addValue(field, 0.01, "Negative Three-phase Reactive Energy (Tariff 2)", "kvarh");
            } else if (fieldMatch(field, {0x84, 0xA0, 0x10, 0xFF, 0x80, 0x84, 0x3B})) {
                addValue(field, 0.01, "Partial Positive Three-phase Active Energy", "kWh");
            } else if (fieldMatch(field, {0x84, 0xA0, 0x10, 0xFF, 0x80, 0x84, 0x3C})) {
                addValue(field, 0.01, "Partial Negative Three-phase Active Energy", "kWh");
            } else if (fieldMatch(field, {0x84, 0xA0, 0x10, 0xFF, 0x81, 0x84, 0x3B})) {
                addValue(field, 0.01, "Partial Positive Three-phase Reactive Energy", "kvarh");
            } else if (fieldMatch(field, {0x84, 0xA0, 0x10, 0xFF, 0x81, 0x84, 0x3C})) {
                addValue(field, 0.01, "Partial Negative Three-phase Reactive Energy", "kvarh");
            } else if (fieldMatch(field, {0x04, 0xFF, 0x90, 0x29})) {
                addValue(field, 0.01, "Pulse Input", "units");
            } else if (fieldMatch(field, {0x02, 0xFF, 0x91, 0x2B})) {
                addValue(field, 1, "Pulse Unit", "units");
            } else if (fieldMatch(field, {0x02, 0xFF, 0x92, 0x2B})) {
                addValue(field, 1, "KTA (Current Transformer Ratio)", "units");
            } else if (fieldMatch(field, {0x02, 0xFF, 0x93, 0x29})) {
                addValue(field, 0.01, "KTV (Voltage Transformer Ratio)", "units");
            }

            // Telegram 2
            else if (fieldMatch(field, {0x84, 0xB0, 0x10, 0xFF, 0x84, 0x2B})) {
                addValue(field, 1, "Three-phase Total Active Power", "W");
            } else if (fieldMatch(field, {0x84, 0x80, 0x20, 0xFF, 0x84, 0x2B})) {
                addValue(field, 1, "Active Power L1", "W");
            } else if (fieldMatch(field, {0x84, 0x90, 0x20, 0xFF, 0x84, 0x2B})) {
                addValue(field, 1, "Active Power L2", "W");
            } else if (fieldMatch(field, {0x84, 0xA0, 0x20, 0xFF, 0x84, 0x2B})) {
                addValue(field, 1, "Active Power L3", "W");
            } else if (fieldMatch(field, {0x84, 0xB0, 0x10, 0xFF, 0x85, 0x2B})) {
                addValue(field, 1, "Three-phase Total Reactive Power", "var");
            } else if (fieldMatch(field, {0x84, 0x80, 0x20, 0xFF, 0x85, 0x2B})) {
                addValue(field, 1, "Reactive Power L1", "var");
            } else if (fieldMatch(field, {0x84, 0x90, 0x20, 0xFF, 0x85, 0x2B})) {
                addValue(field, 1, "Reactive Power L2", "var");
            } else if (fieldMatch(field, {0x84, 0xA0, 0x20, 0xFF, 0x85, 0x2B})) {
                addValue(field, 1, "Reactive Power L3", "var");
            } else if (fieldMatch(field, {0x84, 0xB0, 0x10, 0xFF, 0x86, 0x2B})) {
                addValue(field, 1, "Three-phase Total Apparent Power", "VA");
            } else if (fieldMatch(field, {0x84, 0x80, 0x20, 0xFF, 0x86, 0x2B})) {
                addValue(field, 1, "Apparent Power L1", "VA");
            } else if (fieldMatch(field, {0x84, 0x90, 0x20, 0xFF, 0x86, 0x2B})) {
                addValue(field, 1, "Apparent Power L2", "VA");
            } else if (fieldMatch(field, {0x84, 0xA0, 0x20, 0xFF, 0x86, 0x2B})) {
                addValue(field, 1, "Apparent Power L3", "VA");
            }

            // Telegram 3
            else if (fieldMatch(field, {0x84, 0x80, 0x20, 0xFF, 0x87, 0x48})) {
                addValue(field, 0.1, "1-N Voltage", "V");
            } else if (fieldMatch(field, {0x84, 0x90, 0x20, 0xFF, 0x87, 0x48})) {
                addValue(field, 0.1, "2-N Voltage", "V");
            } else if (fieldMatch(field, {0x84, 0xA0, 0x20, 0xFF, 0x87, 0x48})) {
                addValue(field, 0.1, "3-N Voltage", "V");
            } else if (fieldMatch(field, {0x84, 0x80, 0x20, 0xFF, 0x88, 0x48})) {
                addValue(field, 0.1, "1-2 Voltage", "V");
            } else if (fieldMatch(field, {0x84, 0x90, 0x20, 0xFF, 0x88, 0x48})) {
                addValue(field, 0.1, "2-3 Voltage", "V");
            } else if (fieldMatch(field, {0x84, 0xA0, 0x20, 0xFF, 0x88, 0x48})) {
                addValue(field, 0.1, "3-1 Voltage", "V");
            } else if (fieldMatch(field, {0x84, 0x80, 0x20, 0xFF, 0x89, 0x59})) {
                addValue(field, 0.001, "Phase 1 Current Value", "A");
            } else if (fieldMatch(field, {0x84, 0x90, 0x20, 0xFF, 0x89, 0x59})) {
                addValue(field, 0.001, "Phase 2 Current Value", "A");
            } else if (fieldMatch(field, {0x84, 0xA0, 0x20, 0xFF, 0x89, 0x59})) {
                addValue(field, 0.001, "Phase 3 Current Value", "A");
            } else if (fieldMatch(field, {0x02, 0xFF, 0x8A, 0x48})) {
                addValue(field, 0.1, "Frequency", "Hz");
            }

            // Telegram 4
            else if (fieldMatch(field, {0x82, 0xB0, 0x10, 0xFF, 0x8B, 0x28})) {
                addValue(field, 0.001, "Three-phase Power Factor (PF)", "");
            } else if (fieldMatch(field, {0x82, 0xB0, 0x10, 0xFF, 0x8C, 0x2B})) {
                addValue(field, 1, "Power Factor (PF) sector", ""); // Note: Process based on value for 0, 1, 2
            } else if (fieldMatch(field, {0x84, 0x10, 0xFF, 0x8D, 0x2B})) {
                addValue(field, 1, "Total Active Power Requirement (MD)", "W");
            } else if (fieldMatch(field, {0x84, 0x10, 0xFF, 0x8E, 0x2B})) {
                addValue(field, 1, "Maximum Total Active Power Requirement Tariff 1 (PMD T1)", "W");
            } else if (fieldMatch(field, {0x84, 0x20, 0xFF, 0x8E, 0x2B})) {
                addValue(field, 1, "Maximum Total Active Power Requirement Tariff 2 (PMD T2)", "W");
            } else if (fieldMatch(field, {0x84, 0xB0, 0x10, 0xFF, 0x8F, 0x21})) {
                addValue(field, 1, "Run hour meter (TOT)", "min");
            } else if (fieldMatch(field, {0x84, 0x10, 0xFF, 0x8F, 0x21})) {
                addValue(field, 1, "Run hour meter (Tariff 1)", "min");
            } else if (fieldMatch(field, {0x84, 0x20, 0xFF, 0x8F, 0x21})) {
                addValue(field, 1, "Run hour meter (Tariff 2)", "min");
            }
        }
    }

private:
    bool fieldMatch(const std::vector<uint8_t>& field, const std::vector<uint8_t>& signature) {
        if (field.size() < signature.size()) return false;
        return std::equal(signature.begin(), signature.end(), field.begin());
    }

    void addValue(const std::vector<uint8_t>& field, double scale, const std::string& description, const std::string& unit) {
        // This is a simplified example of how you might convert raw data and store it
        int rawValue = (field[7] << 24) | (field[6] << 16) | (field[5] << 8) | field[4];  // Adjust byte order and width as necessary
        double value = rawValue * scale;
        std::cout << description << ": " << value << " " << unit << std::endl;
    }

    std::string decodeSecondaryAddress(const std::vector<uint8_t>& data) {
        std::ostringstream oss;
        for (int i = 7; i >= 0; --i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
        }
        return oss.str();
    }
};

/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"formula.h"
#include"formula_implementation.h"
#include"meters.h"
#include"units.h"

#include<cmath>
#include<string.h>

NumericFormula::~NumericFormula()
{
}

NumericFormulaConstant::~NumericFormulaConstant()
{
}

NumericFormulaField::~NumericFormulaField()
{
}

NumericFormulaAddition::~NumericFormulaAddition()
{
}

double NumericFormulaConstant::calculate(Unit to)
{
    return convert(constant_, unit(), to);
}

double NumericFormulaField::calculate(Unit to)
{
    return meter_->getNumericValue(field_info_, to);
}

double NumericFormulaAddition::calculate(Unit to)
{
    double sum = 0;

    sum += left_->calculate(to);
    sum += right_->calculate(to);

    return sum;
}

const char *toString(TokenType tt)
{
    switch (tt) {
    case TokenType::SPACE: return "SPACE";
    case TokenType::NUMBER: return "NUMBER";
    case TokenType::LPAR: return "LPAR";
    case TokenType::RPAR: return "RPAR";
    case TokenType::PLUS: return "PLUS";
    case TokenType::UNIT: return "UNIT";
    case TokenType::FIELD: return "FIELD";
    }
    return "?";
}

string Token::str(const string &s)
{
    string v = s.substr(start, len);
    return tostrprintf("%s(%s)", toString(type), v.c_str());
}

double Token::val(const string &s)
{
    string v = s.substr(start, len);
    return atof(v.c_str());
}

string Token::vals(const string &s)
{
    return s.substr(start, len);
}

Unit Token::unit(const string &s)
{
    string v = s.substr(start, len);
    return toUnit(v);
}

void FormulaImplementation::clear()
{
    valid_ = true;
    while (!ops_.empty()) ops_.pop();
    tokens_.clear();
    formula_ = "";
    meter_ = NULL;
}

size_t FormulaImplementation::findSpace(size_t i)
{
    size_t len = 0;
    while(isspace(formula_[i]))
    {
        i++;
        len++;
    }
    return len;
}

size_t FormulaImplementation::findNumber(size_t i)
{
    size_t len = 0;
    size_t start = i;
    int num_dots = 0;
    while (true)
    {
        if (i >= formula_.length()) break;
        char c = formula_[i];
        if ((c < '0' || c > '9') && c != '.') break;
        if (c == '.')
        {
            if (start == i) return 0; // Numbers do not start with a dot.
            num_dots++;
        }
        i++;
        len++;
    }

    if (num_dots > 1) return 0; // More than one decimal dot is an error.

    return len;
}

size_t FormulaImplementation::findPlus(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '+') return 1;

    return 0;
}

size_t FormulaImplementation::findLPar(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '(') return 1;

    return 0;
}

size_t FormulaImplementation::findRPar(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == ')') return 1;

    return 0;
}

bool is_letter(char c)
{
    return c >= 'a' && c <= 'z';
}

bool is_letter_or_underscore(char c)
{
    return c == '_' || (c >= 'a' && c <= 'z');
}

bool is_letter_digit_or_underscore(char c)
{
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

size_t FormulaImplementation::findUnit(size_t i)
{
    if (i >= formula_.length()) return 0;

    size_t len = formula_.length();
    char c = formula_[i];

    // All units start with a lower case a-z, followed by more letters and _ underscores.
    if (!is_letter(c)) return 0;

#define X(cname,lcname,hrname,quantity,explanation)                     \
    if ( (i+sizeof(#lcname)-1 <= len) &&                                  \
         !is_letter_or_underscore(formula_[i+sizeof(#lcname)-1]) &&                     \
         !strncmp(#lcname, formula_.c_str()+i, sizeof(#lcname)-1)) return sizeof(#lcname)-1;
LIST_OF_UNITS
#undef X

    return 0;
}

size_t FormulaImplementation::findField(size_t i)
{
    size_t len = 0;
    size_t start = i;

    // All field names are lower case a-z.
    if (!is_letter(formula_[start])) return 0;

    while (true)
    {
        if (i >= formula_.length()) break;
        char c = formula_[i];
        // After first letter field names can contain more letters digits and underscores.
        if (!is_letter_digit_or_underscore(c)) break;
        i++;
        len++;
    }

    return len;
}

bool FormulaImplementation::tokenize()
{
    size_t i = 0;

    while (i<formula_.length())
    {
        size_t len;

        len = findSpace(i);
        if (len > 0) { i+=len; continue; } // No token added for whitespace.

        len = findNumber(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::NUMBER, i, len)); i+=len; continue; }

        len = findLPar(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::LPAR, i, len)); i+=len; continue; }

        len = findRPar(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::RPAR, i, len)); i+=len; continue; }

        len = findPlus(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::PLUS, i, len)); i+=len; continue; }

        len = findUnit(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::UNIT, i, len)); i+=len; continue; }

        len = findField(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::FIELD, i, len)); i+=len; continue; }

        break;
    }

    // Interrupted early, thus there was an error tokenizing.
    if (i < formula_.length()) return false;

    return true;
}

size_t FormulaImplementation::parseOps(size_t i)
{
    Token *tok = LA(i);
    Token *next= LA(i+1);
    if (tok == NULL) return i;

    if (tok->type == TokenType::FIELD)
    {
        handleField(tok);
        return i+1;
    }

    if (tok->type == TokenType::PLUS)
    {
        size_t next = parseOps(i+1);
        handleAddition();
        return next;
    }

    if (tok->type == TokenType::LPAR)
    {
        return parsePar(i);
    }

    if (tok->type == TokenType::RPAR)
    {
        return i;
    }

    if (next == NULL) return i;

    if (tok->type == TokenType::NUMBER && next->type == TokenType::UNIT)
    {
        handleConstant(tok, next);
        return i+2;
    }

    return i;
}

size_t FormulaImplementation::parsePar(size_t i)
{
    Token *tok = LA(i);
    assert(tok->type == TokenType::LPAR);

    i++;
    for (;;)
    {
        tok = LA(i);
        if (tok == NULL) break;
        if (tok->type == TokenType::RPAR) break;
        size_t next = parseOps(i);
        if (next == i) break;
        i = next;
    }

    if (tok == NULL)
    {
        warning("No closing parenthesis found!\n");
        return i;
    }

    if (tok->type != TokenType::RPAR)
    {
        warning("Expected parenthesis but got xx intead.\n");
        return i;
    }
    return i+1;
}

void FormulaImplementation::handleConstant(Token *number, Token *unit)
{
    double c = number->val(formula_);
    Unit u = unit->unit(formula_);

    if (u == Unit::Unknown)
    {
        warning("Unknown unit \"%s\" in formula:\n%s\n", unit->vals(formula_).c_str(), formula_.c_str());
        return;
    }
    //debug("(formula) push constant %f %s\n", c, unitToStringLowerCase(u).c_str());
    doConstant(u, c);
}

void FormulaImplementation::handleAddition()
{
    //debug("(formula) push addition\n");
    doAddition();
}

void FormulaImplementation::handleField(Token *field)
{
    string field_name = field->vals(formula_); // Full field: total_m3
    //debug("(formula) push field %s\n", field_name.c_str());
    string vname;                      // Without unit: total
    Unit unit;
    bool ok = extractUnit(field_name, &vname, &unit);

    debug("(formula) handle field %s into %s %s\n", field_name.c_str(), vname.c_str(), unitToStringLowerCase(unit).c_str());

    if (!ok)
    {
        warning("Could not extract a valid unit from field name \"%s\"\n", field_name.c_str());
        return;
    }

    Quantity q = toQuantity(unit);
    FieldInfo *f = meter_->findFieldInfo(vname, q);

    if (f == NULL)
    {
        warning("No such field found \"%s\" (%s %s)\n", field_name.c_str(), vname.c_str(), toString(q));
        return;
    }

    ok = doField(unit, meter_, f);
    if (!ok)
    {
        warning("Could not use field \"%s\" (%s %s)\n", field_name.c_str(), vname.c_str(), toString(q));
        return;
    }
}

bool FormulaImplementation::go()
{
    size_t i = 0;

    for (;;)
    {
        size_t next = parseOps(i);
        if (next == i) break;
        i = next;
    }

    return i == tokens_.size();
}

Token *FormulaImplementation::LA(size_t i)
{
    if (i < 0 || i >= tokens_.size()) return NULL;
    return &tokens_[i];
}

bool FormulaImplementation::parse(Meter *m, const string &f)
{
    bool ok;

    meter_ = m;
    formula_ = f;

    debug("(formula) parsing \"%s\"\n", formula_.c_str());

    ok = tokenize();
    if (!ok) return false;

    if (isDebugEnabled())
    {
        debug("(formula) tokens: ");
        for (Token &t : tokens_)
        {
            debug("%s ", t.str(formula_).c_str());
        }
        debug("\n");
    }

    ok = go();
    if (!ok) return false;

    if (isDebugEnabled())
    {
        debug("(formula) %s\n", tree().c_str());
    }
    return true;
}

bool FormulaImplementation::valid()
{
    return valid_ == true && ops_.size() == 1;
}

double FormulaImplementation::calculate(Unit to)
{
    if (!valid_)
    {
        warning("(formula) not valid returning nan!\n");
        return std::nan("");
    }
    if (ops_.size() != 1)
    {
        warning("(formula) does not have a single op (has %zu), not valid returning nan!\n", ops_.size());
        return std::nan("");
    }

    return ops_.top().get()->calculate(to);
}

bool FormulaImplementation::doConstant(Unit u, double c)
{
    ops_.push(unique_ptr<NumericFormula>(new NumericFormulaConstant(u, c)));
    return true;
}

bool FormulaImplementation::doAddition()
{
    if (ops_.size() < 2) { valid_ = false; return false;}

    Unit right_unit = ops_.top()->unit();

    unique_ptr<NumericFormula> right_node = std::move(ops_.top());
    ops_.pop();

    Unit left_unit = ops_.top()->unit();

    unique_ptr<NumericFormula> left_node = std::move(ops_.top());
    ops_.pop();

    ops_.push(unique_ptr<NumericFormula>(new NumericFormulaAddition(left_unit, left_node, right_node)));

    if (!canConvert(left_unit, right_unit))
    {
        valid_ = false;
        return false;
    }

    return true;
}

bool FormulaImplementation::doField(Unit u, Meter *m, FieldInfo *fi)
{
    if (!canConvert(u, fi->defaultUnit()))
    {
        valid_ = false;
        return false;
    }
    ops_.push(unique_ptr<NumericFormula>(new NumericFormulaField(u, m, fi)));
    return true;
}

Formula *newFormula()
{
    return new FormulaImplementation();
}

Formula::~Formula()
{
}

FormulaImplementation::~FormulaImplementation()
{
}

string FormulaImplementation::str()
{
    return ops_.top()->str();
}

string FormulaImplementation::tree()
{
    string t = ops_.top()->tree();
    if (t.back() == ' ') t.pop_back();
    return t;
}

string NumericFormulaConstant::str()
{
    return tostrprintf("%.15g %s", constant_, unit());
}

string NumericFormulaConstant::tree()
{
    return tostrprintf("<CONST %.15g %s> ", constant_, unitToStringLowerCase(unit()).c_str());
}

string NumericFormulaAddition::str()
{
    string left = left_->tree();
    string right = right_->tree();
    return left+" + "+right;
}

string NumericFormulaAddition::tree()
{
    string left = left_->tree();
    string right = right_->tree();
    return "<ADD "+left+right+"> ";
}

string NumericFormulaField::str()
{
    return field_info_->vname()+"_"+unitToStringLowerCase(field_info_->defaultUnit());
}

string NumericFormulaField::tree()
{
    return "<FIELD "+field_info_->vname()+"_"+unitToStringLowerCase(field_info_->defaultUnit())+"> ";
}

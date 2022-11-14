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

NumericFormula::~NumericFormula() { }
NumericFormulaConstant::~NumericFormulaConstant() { }
NumericFormulaField::~NumericFormulaField() { }
NumericFormulaPair::~NumericFormulaPair() { }
NumericFormulaAddition::~NumericFormulaAddition() { }
NumericFormulaSubtraction::~NumericFormulaSubtraction() { }
NumericFormulaMultiplication::~NumericFormulaMultiplication() { }
NumericFormulaDivision::~NumericFormulaDivision() { }
NumericFormulaExponentiation::~NumericFormulaExponentiation() { }
NumericFormulaSquareRoot::~NumericFormulaSquareRoot() { }

double NumericFormulaConstant::calculate(SIUnit to)
{
    return siunit().convertTo(constant_, to);
}

double NumericFormulaField::calculate(SIUnit to_si_unit)
{
    Unit field_unit = field_info_->defaultUnit();
    double val = meter_->getNumericValue(field_info_, field_unit);

    const SIUnit& field_si_unit = toSIUnit(field_unit);

    return field_si_unit.convertTo(val, to_si_unit);
}

double NumericFormulaAddition::calculate(SIUnit to)
{
    double l = left_->calculate(to);
    double r = right_->calculate(to);

    return l+r;
}

double NumericFormulaSubtraction::calculate(SIUnit to)
{
    double l = left_->calculate(to);
    double r = right_->calculate(to);

    return l-r;
}

double NumericFormulaMultiplication::calculate(SIUnit to)
{
    double l = left_->calculate(left_->siunit());
    double r = right_->calculate(right_->siunit());
    double m = l*r;
    double v = siunit().convertTo(m, to);

    if (isDebugEnabled())
    {
        debug("(formula) MUL %g (%s) %g (%s) --> %g --> %g %s\n",
              l, left_->siunit().info().c_str(),
              r, right_->siunit().info().c_str(),
              m,
              v, to.info().c_str());
    }
    return v;
}

double NumericFormulaDivision::calculate(SIUnit to)
{
    double l = left_->calculate(left_->siunit());
    double r = right_->calculate(right_->siunit());
    double d = l/r;
    double v = siunit().convertTo(d, to);

    if (isDebugEnabled())
    {
        debug("(formula) DIV %g (%s) %g (%s) --> %g --> %g %s\n",
              l, left_->siunit().info().c_str(),
              r, right_->siunit().info().c_str(),
              d,
              v,
              to.info().c_str());
    }

    return v;
}

double NumericFormulaExponentiation::calculate(SIUnit to)
{
    double l = left_->calculate(to);
    double r = right_->calculate(to);
    double p = pow(l,r);
    double v = siunit().convertTo(p, to);

    debug("(formula) %g <-- %g <-- pow %g ^ %g\n", v, p, l, r);
    return v;
}

double NumericFormulaSquareRoot::calculate(SIUnit to)
{
    double i = inner_->calculate(inner_->siunit());
    double s = sqrt(i);
    double v = siunit().convertTo(s, to);

    if (isDebugEnabled())
    {
        debug("(formula) SQRT %g (%s) --> %g --> %g %s\n",
              i, inner_->siunit().info().c_str(),
              s,
              v,
              to.info().c_str());
    }
    return v;
}

const char *toString(TokenType tt)
{
    switch (tt) {
    case TokenType::SPACE: return "SPACE";
    case TokenType::NUMBER: return "NUMBER";
    case TokenType::LPAR: return "LPAR";
    case TokenType::RPAR: return "RPAR";
    case TokenType::PLUS: return "PLUS";
    case TokenType::MINUS: return "MINUS";
    case TokenType::TIMES: return "TIMES";
    case TokenType::DIV: return "DIV";
    case TokenType::EXP: return "EXP";
    case TokenType::SQRT: return "SQRT";
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
    op_stack_.clear();
    tokens_.clear();
    formula_ = "";
    meter_ = NULL;
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

size_t FormulaImplementation::findMinus(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '-') return 1;

    return 0;
}

size_t FormulaImplementation::findTimes(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '*') return 1;

    return 0;
}

size_t FormulaImplementation::findDiv(size_t i)
{
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '/') return 1;

    return 0;
}

size_t FormulaImplementation::findExp(size_t i)
{
    return 0;
    /*
    if (i >= formula_.length()) return 0;

    char c = formula_[i];
    if (c == '^') return 1;

    return 0;
    */
}

size_t FormulaImplementation::findSqrt(size_t i)
{
    if (i+4 >= formula_.length()) return 0;

    if (!strncmp(&formula_[i], "sqrt", 4) && !is_letter(formula_[i+4]))
    {
        return 4;
    }

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

size_t FormulaImplementation::findUnit(size_t i)
{
    if (i >= formula_.length()) return 0;

    size_t len = formula_.length();
    char c = formula_[i];

    // All units start with a lower case a-z, followed by more letters and _ underscores.
    if (!is_letter(c)) return 0;

#define X(cname,lcname,hrname,quantity,explanation) \
    if ( (i+sizeof(#lcname)-1 <= len) &&                                \
         !is_letter_or_underscore(formula_[i+sizeof(#lcname)-1]) &&     \
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

        len = findMinus(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::MINUS, i, len)); i+=len; continue; }

        len = findTimes(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::TIMES, i, len)); i+=len; continue; }

        len = findDiv(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::DIV, i, len)); i+=len; continue; }

        len = findExp(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::EXP, i, len)); i+=len; continue; }

        len = findSqrt(i);
        if (len > 0) { tokens_.push_back(Token(TokenType::SQRT, i, len)); i+=len; continue; }

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
        handleAddition(tok);
        return next;
    }

    if (tok->type == TokenType::MINUS)
    {
        size_t next = parseOps(i+1);
        handleSubtraction(tok);
        return next;
    }

    if (tok->type == TokenType::TIMES)
    {
        size_t next = parseOps(i+1);
        handleMultiplication(tok);
        return next;
    }

    if (tok->type == TokenType::DIV)
    {
        size_t next = parseOps(i+1);
        handleDivision(tok);
        return next;
    }

    if (tok->type == TokenType::EXP)
    {
        size_t next = parseOps(i+1);
        handleExponentiation(tok);
        return next;
    }

    if (tok->type == TokenType::SQRT)
    {
        size_t next = parseOps(i+1);
        handleSquareRoot(tok);
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
        errors_.push_back(tostrprintf("Missing closing parenthesis at end of formula!\n"));
        valid_ = false;
        return i;
    }

    if (tok->type != TokenType::RPAR)
    {
        errors_.push_back("Expected closing parenthesis!\n"+tok->withMarker(formula_));
        valid_ = false;
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
        errors_.push_back(tostrprintf("Unknown unit \"%s\"!\n%s",
                                      unit->vals(formula_).c_str(),
                                      unit->withMarker(formula_).c_str()));
        valid_ = false;
        return;
    }

    doConstant(u, c);
}

void FormulaImplementation::handleAddition(Token *tok)
{
    SIUnit right_siunit = topOp()->siunit();
    SIUnit left_siunit = top2Op()->siunit();

    if (!left_siunit.canConvertTo(right_siunit))
    {
        string lsis = left_siunit.str();
        string rsis = right_siunit.str();
        errors_.push_back(tostrprintf("Cannot add %s to %s!\n%s",
                                      left_siunit.info().c_str(),
                                      right_siunit.info().c_str(),
                                      tok->withMarker(formula_).c_str()));
        valid_ = false;
        return;
    }

    doAddition();
}

void FormulaImplementation::handleSubtraction(Token *tok)
{
    SIUnit right_siunit = topOp()->siunit();
    SIUnit left_siunit = top2Op()->siunit();

    if (!left_siunit.canConvertTo(right_siunit))
    {
        string lsis = left_siunit.str();
        string rsis = right_siunit.str();
        errors_.push_back(tostrprintf("Cannot subtract %s to %s!\n%s",
                                      left_siunit.info().c_str(),
                                      right_siunit.info().c_str(),
                                      tok->withMarker(formula_).c_str()));
        valid_ = false;
        return;
    }

    doSubtraction();
}

void FormulaImplementation::handleMultiplication(Token *tok)
{
    // Any two units can be multiplied! You might not like the answer thought....
    doMultiplication();
}

void FormulaImplementation::handleDivision(Token *tok)
{
    // Any two units can be multiplied! You might not like the answer thought....
    doDivision();
}

void FormulaImplementation::handleExponentiation(Token *tok)
{
    // You can only exponentiate to a number.
    doExponentiation();
}

void FormulaImplementation::handleSquareRoot(Token *tok)
{
    doSquareRoot();
}

void FormulaImplementation::handleField(Token *field)
{
    string field_name = field->vals(formula_); // Full field: total_m3
    string vname;                              // Without unit: total
    Unit named_unit;                      // The extracted unit: m3
    bool ok = extractUnit(field_name, &vname, &named_unit);

    debug("(formula) handle field %s into %s %s\n", field_name.c_str(), vname.c_str(), unitToStringLowerCase(named_unit).c_str());

    if (!ok)
    {
        errors_.push_back("Cannot extra a valid unit from field name \""+field_name+"\"\n"+field->withMarker(formula_));
        return;
    }

    Quantity q = toQuantity(named_unit);
    FieldInfo *f = meter_->findFieldInfo(vname, q);

    if (f == NULL)
    {
        errors_.push_back("No such field found \""+field_name+"\"\n"+field->withMarker(formula_));
        valid_ = false;
        return;
    }

    doField(named_unit, meter_, f);
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
    return valid_;
}

bool FormulaImplementation::valid()
{
    return valid_ == true && op_stack_.size() == 1;
}

string FormulaImplementation::errors()
{
    string s;
    for (string& e : errors_) s += e;
    return s;
}

double FormulaImplementation::calculate(Unit to)
{
    if (!valid_)
    {
        string t = tree();
        warning("Warning! Formula is not valid! Returning nan!\n%s\n", t.c_str());
        return std::nan("");
    }

    if (op_stack_.size() != 1)
    {
        string t = tree();
        warning("Warning! Formula is not valid! Multiple ops on stack! Returning nan!\n%s\n", t.c_str());
        return std::nan("");
    }

    return topOp()->calculate(toSIUnit(to));
}

void FormulaImplementation::doConstant(Unit u, double c)
{
    pushOp(new NumericFormulaConstant(u, c));
}

void FormulaImplementation::doAddition()
{
    assert(op_stack_.size() >= 2);

    SIUnit right_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> right_node = popOp();

    SIUnit left_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> left_node = popOp();

    pushOp(new NumericFormulaAddition(left_siunit, left_node, right_node));

    assert(left_siunit.canConvertTo(right_siunit));
}

void FormulaImplementation::doSubtraction()
{
    assert(op_stack_.size() >= 2);

    SIUnit right_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> right_node = popOp();

    SIUnit left_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> left_node = popOp();

    pushOp(new NumericFormulaSubtraction(left_siunit, left_node, right_node));

    assert(left_siunit.canConvertTo(right_siunit));
}

void FormulaImplementation::doMultiplication()
{
    assert(op_stack_.size() >= 2);

    SIUnit right_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> right_node = popOp();

    SIUnit left_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> left_node = popOp();

    SIUnit mul_siunit = left_siunit.mul(right_siunit);

    string lsis = left_siunit.info();
    string rsis = right_siunit.info();
    string msis = mul_siunit.info();

    pushOp(new NumericFormulaMultiplication(mul_siunit, left_node, right_node));
}

void FormulaImplementation::doDivision()
{
    assert(op_stack_.size() >= 2);

    SIUnit right_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> right_node = popOp();

    SIUnit left_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> left_node = popOp();

    SIUnit div_siunit = left_siunit.div(right_siunit);

    string lsis = left_siunit.info();
    string rsis = right_siunit.info();
    string dsis = div_siunit.info();

    debug("(formula) unit %s DIV %s ==> %s\n", lsis.c_str(), rsis.c_str(), dsis.c_str());

    pushOp(new NumericFormulaDivision(div_siunit, left_node, right_node));
}

void FormulaImplementation::doExponentiation()
{
    assert(op_stack_.size() >= 2);

//    SIUnit right_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> right_node = popOp();

    SIUnit left_siunit = topOp()->siunit();

    unique_ptr<NumericFormula> left_node = popOp();

    pushOp(new NumericFormulaDivision(left_siunit, left_node, right_node));

//    assert(canConvert(left_siunit, right_siunit));
}

void FormulaImplementation::doSquareRoot()
{
    assert(op_stack_.size() >= 1);

    SIUnit inner_siunit = topOp()->siunit();

    SIUnit siunit = inner_siunit.sqrt();

    unique_ptr<NumericFormula> inner_node = popOp();

    pushOp(new NumericFormulaSquareRoot(siunit, inner_node));
}


void FormulaImplementation::doField(Unit u, Meter *m, FieldInfo *fi)
{
    SIUnit from_si_unit = toSIUnit(fi->defaultUnit());
    SIUnit to_si_unit = toSIUnit(u);
    assert(from_si_unit.canConvertTo(to_si_unit));
    pushOp(new NumericFormulaField(u, m, fi));
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
    return topOp()->str();
}

string FormulaImplementation::tree()
{
    string s;

    for (auto &op : op_stack_)
    {
        if (s.length() > 0) s += " | ";
        s += op->tree();
        if (s.back() == ' ') s.pop_back();
    }
    return s;
}

string NumericFormulaConstant::str()
{
    return tostrprintf("%.17g %s", constant_, siunit());
}

string NumericFormulaConstant::tree()
{
    Quantity q = siunit().quantity();
    Unit nearest = siunit().asUnit(q);
    string sis = siunit().str();

    return tostrprintf("<CONST %.17g %s[%s]%s> ", constant_, unitToStringLowerCase(nearest).c_str(), sis.c_str(), toString(q));
}

string NumericFormulaPair::str()
{
    string left = left_->tree();
    string right = right_->tree();
    return left+" "+op_+" "+right;
}

string NumericFormulaPair::tree()
{
    string left = left_->tree();
    string right = right_->tree();
    return "<"+name_+" "+left+right+"> ";
}

string NumericFormulaSquareRoot::str()
{
    string inner = inner_->str();
    return "sqrt("+inner+")";
}

string NumericFormulaSquareRoot::tree()
{
    string inner = inner_->tree();
    return "<SQRT "+inner+"> ";
}

string NumericFormulaField::str()
{
    return field_info_->vname()+"_"+unitToStringLowerCase(field_info_->defaultUnit());
}

string NumericFormulaField::tree()
{
    return "<FIELD "+field_info_->vname()+"_"+unitToStringLowerCase(field_info_->defaultUnit())+"> ";
}

void FormulaImplementation::pushOp(NumericFormula *nf)
{
    op_stack_.push_back(unique_ptr<NumericFormula>(nf));
}

unique_ptr<NumericFormula> FormulaImplementation::popOp()
{
    assert(op_stack_.size() > 0);
    unique_ptr<NumericFormula> nf = std::move(op_stack_.back());
    op_stack_.pop_back();
    return nf;
}

NumericFormula * FormulaImplementation::topOp()
{
    assert(op_stack_.size() > 0);
    return op_stack_.back().get();
}

NumericFormula * FormulaImplementation::top2Op()
{
    assert(op_stack_.size() > 1);
    return op_stack_[op_stack_.size()-2].get();
}

string Token::withMarker(const string& formula)
{
    string indent;
    size_t n = start;
    while (n > 0)
    {
        indent += " ";
        n--;
    }
    return formula+"\n"+indent+"^~~~~\n";
}

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

#ifndef FORMULA_IMPLEMENTATION
#define FORMULA_IMPLEMENTATION

#include"formula.h"
#include"meters.h"

#include<stack>

struct NumericFormula
{
    NumericFormula(Unit u) : unit_(u) { }
    Unit unit() { return unit_; }
    virtual double calculate(Unit to) = 0;
    virtual string str() = 0;
    virtual string tree() = 0;
    virtual ~NumericFormula() = 0;

    private:

    Unit unit_;
};

struct NumericFormulaConstant : public NumericFormula
{
    NumericFormulaConstant(Unit u, double c) : NumericFormula(u), constant_(c) {}
    double calculate(Unit to);
    string str();
    string tree();
    ~NumericFormulaConstant();

    private:

    double constant_;
};

struct NumericFormulaField : public NumericFormula
{
    NumericFormulaField(Unit u, Meter *m, FieldInfo *fi) : NumericFormula(u), meter_(m), field_info_(fi) {}
    double calculate(Unit to);
    string str();
    string tree();
    ~NumericFormulaField();

    private:

    Meter *meter_;
    FieldInfo *field_info_;
};

struct NumericFormulaAddition : public NumericFormula
{
    NumericFormulaAddition(Unit u,
                           unique_ptr<NumericFormula> &a,
                           unique_ptr<NumericFormula> &b)
        : NumericFormula(u), left_(std::move(a)), right_(std::move(b)) {}

    double calculate(Unit to);
    string str();
    string tree();

    ~NumericFormulaAddition();

private:

    std::unique_ptr<NumericFormula> left_;
    std::unique_ptr<NumericFormula> right_;
};

enum class TokenType
{
    SPACE,
    LPAR,
    RPAR,
    NUMBER,
    PLUS,
    UNIT,
    FIELD
};

const char *toString(TokenType tt);

struct Token
{
    Token(TokenType t, size_t s, size_t l) : type(t), start(s), len(l) {}

    TokenType type;
    size_t start;
    size_t len;

    string str(const string &s);
    string vals(const string &s);
    double val(const string &s);
    Unit unit(const string &s);
};

struct FormulaImplementation : public Formula
{
    bool parse(Meter *m, const string &f);
    bool valid();
    double calculate(Unit to);
    void clear();
    string str();
    string tree();

    // Pushes a constant on the stack.
    bool doConstant(Unit u, double c);
    // Pushes a field read on the stack. Returns false if the field is not found in the meter.
    bool doField(Unit u, Meter *m, FieldInfo *fi);
    // Pops the two top nodes of the stack and pushes an addition (using these members) on the stack.
    // The target unit will be the first unit of the two operands. If incompatible units, then it will
    // return false.
    bool doAddition();

    ~FormulaImplementation();

private:

    bool tokenize();
    bool go();
    size_t findSpace(size_t i);
    size_t findNumber(size_t i);
    size_t findUnit(size_t i);
    size_t findPlus(size_t i);
    size_t findLPar(size_t i);
    size_t findRPar(size_t i);
    size_t findField(size_t i);

    Token *LA(size_t i);
    size_t parseOps(size_t i);
    size_t parsePar(size_t i);

    void handleConstant(Token *number, Token *unit);
    void handleAddition();
    void handleField(Token *field);

    bool valid_ = true;
    std::stack<std::unique_ptr<NumericFormula>> ops_;
    std::vector<Token> tokens_;
    std::string formula_; // To be parsed.
    Meter *meter_; // To be referenced when parsing.
};

#endif

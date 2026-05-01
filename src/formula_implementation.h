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

struct FormulaImplementation;

struct NumericFormula
{
    NumericFormula(FormulaImplementation *f, SIUnit u) : formula_(f), siunit_(u) { }
    SIUnit &siunit() { return siunit_; }
    // Calculate the formula and return the value in the given "to" unit.
    virtual double calculate(SIUnit to) = 0;
    virtual std::string str() = 0;
    virtual std::string tree() = 0;
    virtual ~NumericFormula() = 0;

    FormulaImplementation *formula() { return formula_; }

    private:

    FormulaImplementation *formula_;
    SIUnit siunit_;
};

struct NumericFormulaConstant : public NumericFormula
{
    NumericFormulaConstant(FormulaImplementation *f, Unit u, double c) : NumericFormula(f, u), constant_(c) {}
    double calculate(SIUnit to);
    std::string str();
    std::string tree();
    ~NumericFormulaConstant();

    private:

    double constant_;
};

struct NumericFormulaMeterField : public NumericFormula
{
    NumericFormulaMeterField(FormulaImplementation *f, Unit u, std::string v, Quantity q)
        : NumericFormula(f, u), vname_(v), quantity_(q) {}

    double calculate(SIUnit to);
    std::string str();
    std::string tree();
    ~NumericFormulaMeterField();

    private:

    std::string vname_;
    Quantity quantity_;
};

struct NumericFormulaDVEntryField : public NumericFormula
{
    NumericFormulaDVEntryField(FormulaImplementation *f, Unit u, DVEntryCounterType ct) : NumericFormula(f, u), counter_(ct) {}

    double calculate(SIUnit to);
    std::string str();
    std::string tree();
    ~NumericFormulaDVEntryField();

    private:

    DVEntryCounterType counter_;
};

struct NumericFormulaPair : public NumericFormula
{
    NumericFormulaPair(FormulaImplementation *f, SIUnit siu,
                       std::unique_ptr<NumericFormula> &a,
                       std::unique_ptr<NumericFormula> &b,
                       std::string name, std::string op)
        : NumericFormula(f, siu),
        left_(std::move(a)),
        right_(std::move(b)),
        name_(name),
        op_(op)
    {}

    std::string str();
    std::string tree();
    ~NumericFormulaPair();

protected:

    std::unique_ptr<NumericFormula> left_;
    std::unique_ptr<NumericFormula> right_;
    std::string name_;
    std::string op_;
};

struct NumericFormulaAddition : public NumericFormulaPair
{
    NumericFormulaAddition(FormulaImplementation *f, SIUnit siu,
                           std::unique_ptr<NumericFormula> &a,
                           std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "ADD", "+") {}

    double calculate(SIUnit to);

    ~NumericFormulaAddition();
};

struct NumericFormulaSubtraction : public NumericFormulaPair
{
    NumericFormulaSubtraction(FormulaImplementation *f, SIUnit siu,
                              std::unique_ptr<NumericFormula> &a,
                              std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "SUB", "-") {}

    double calculate(SIUnit to);

    ~NumericFormulaSubtraction();
};

struct NumericFormulaMultiplication : public NumericFormulaPair
{
    NumericFormulaMultiplication(FormulaImplementation *f, SIUnit siu,
                                 std::unique_ptr<NumericFormula> &a,
                                 std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "TIMES", "×") {}

    double calculate(SIUnit to);

    ~NumericFormulaMultiplication();
};

struct NumericFormulaDivision : public NumericFormulaPair
{
    NumericFormulaDivision(FormulaImplementation *f, SIUnit siu,
                           std::unique_ptr<NumericFormula> &a,
                           std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "DIV", "÷") {}

    double calculate(SIUnit to);

    ~NumericFormulaDivision();
};

struct NumericFormulaExponentiation : public NumericFormulaPair
{
    NumericFormulaExponentiation(FormulaImplementation *f, SIUnit siu,
                                 std::unique_ptr<NumericFormula> &a,
                                 std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "EXP", "^") {}

    double calculate(SIUnit to);

    ~NumericFormulaExponentiation();
};

struct NumericFormulaSquareRoot : public NumericFormula
{
    NumericFormulaSquareRoot(FormulaImplementation *f, SIUnit siu,
                             std::unique_ptr<NumericFormula> &inner)
        : NumericFormula(f, siu), inner_(std::move(inner)) {}

    double calculate(SIUnit to);
    std::string str();
    std::string tree();

    ~NumericFormulaSquareRoot();

private:

    std::unique_ptr<NumericFormula> inner_;
};

struct NumericFormulaRound : public NumericFormula
{
    NumericFormulaRound(FormulaImplementation *f, SIUnit siu,
                        std::unique_ptr<NumericFormula> &inner)
        : NumericFormula(f, siu), inner_(std::move(inner)) {}

    double calculate(SIUnit to);
    std::string str();
    std::string tree();

    ~NumericFormulaRound();

private:

    std::unique_ptr<NumericFormula> inner_;
};

struct NumericFormulaFloor : public NumericFormula
{
    NumericFormulaFloor(FormulaImplementation *f, SIUnit siu,
                        std::unique_ptr<NumericFormula> &inner)
        : NumericFormula(f, siu), inner_(std::move(inner)) {}

    double calculate(SIUnit to);
    std::string str();
    std::string tree();

    ~NumericFormulaFloor();

private:

    std::unique_ptr<NumericFormula> inner_;
};

struct NumericFormulaCeil : public NumericFormula
{
    NumericFormulaCeil(FormulaImplementation *f, SIUnit siu,
                       std::unique_ptr<NumericFormula> &inner)
        : NumericFormula(f, siu), inner_(std::move(inner)) {}

    double calculate(SIUnit to);
    std::string str();
    std::string tree();

    ~NumericFormulaCeil();

private:

    std::unique_ptr<NumericFormula> inner_;
};

struct NumericFormulaModulo : public NumericFormulaPair
{
    NumericFormulaModulo(FormulaImplementation *f, SIUnit siu,
                         std::unique_ptr<NumericFormula> &a,
                         std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "MOD", "%") {}

    double calculate(SIUnit to);

    ~NumericFormulaModulo();
};

struct NumericFormulaShiftLeft : public NumericFormulaPair
{
    NumericFormulaShiftLeft(FormulaImplementation *f, SIUnit siu,
                            std::unique_ptr<NumericFormula> &a,
                            std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "SHL", "<<") {}

    double calculate(SIUnit to);

    ~NumericFormulaShiftLeft();
};

struct NumericFormulaShiftRight : public NumericFormulaPair
{
    NumericFormulaShiftRight(FormulaImplementation *f, SIUnit siu,
                             std::unique_ptr<NumericFormula> &a,
                             std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "SHR", ">>") {}

    double calculate(SIUnit to);

    ~NumericFormulaShiftRight();
};

struct NumericFormulaEQ : public NumericFormulaPair
{
    NumericFormulaEQ(FormulaImplementation *f, SIUnit siu,
                     std::unique_ptr<NumericFormula> &a,
                     std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "EQ", "==") {}
    double calculate(SIUnit to);
    ~NumericFormulaEQ();
};

struct NumericFormulaNEQ : public NumericFormulaPair
{
    NumericFormulaNEQ(FormulaImplementation *f, SIUnit siu,
                      std::unique_ptr<NumericFormula> &a,
                      std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "NEQ", "!=") {}
    double calculate(SIUnit to);
    ~NumericFormulaNEQ();
};

struct NumericFormulaLT : public NumericFormulaPair
{
    NumericFormulaLT(FormulaImplementation *f, SIUnit siu,
                     std::unique_ptr<NumericFormula> &a,
                     std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "LT", "<") {}
    double calculate(SIUnit to);
    ~NumericFormulaLT();
};

struct NumericFormulaGT : public NumericFormulaPair
{
    NumericFormulaGT(FormulaImplementation *f, SIUnit siu,
                     std::unique_ptr<NumericFormula> &a,
                     std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "GT", ">") {}
    double calculate(SIUnit to);
    ~NumericFormulaGT();
};

struct NumericFormulaLTE : public NumericFormulaPair
{
    NumericFormulaLTE(FormulaImplementation *f, SIUnit siu,
                      std::unique_ptr<NumericFormula> &a,
                      std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "LTE", "<=") {}
    double calculate(SIUnit to);
    ~NumericFormulaLTE();
};

struct NumericFormulaGTE : public NumericFormulaPair
{
    NumericFormulaGTE(FormulaImplementation *f, SIUnit siu,
                      std::unique_ptr<NumericFormula> &a,
                      std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "GTE", ">=") {}
    double calculate(SIUnit to);
    ~NumericFormulaGTE();
};

struct NumericFormulaBitwiseAnd : public NumericFormulaPair
{
    NumericFormulaBitwiseAnd(FormulaImplementation *f, SIUnit siu,
                              std::unique_ptr<NumericFormula> &a,
                              std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "BAND", "&") {}
    double calculate(SIUnit to);
    ~NumericFormulaBitwiseAnd();
};

struct NumericFormulaBitwiseOr : public NumericFormulaPair
{
    NumericFormulaBitwiseOr(FormulaImplementation *f, SIUnit siu,
                             std::unique_ptr<NumericFormula> &a,
                             std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "BOR", "|") {}
    double calculate(SIUnit to);
    ~NumericFormulaBitwiseOr();
};

struct NumericFormulaLogicalAnd : public NumericFormulaPair
{
    NumericFormulaLogicalAnd(FormulaImplementation *f, SIUnit siu,
                              std::unique_ptr<NumericFormula> &a,
                              std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "LAND", "&&") {}
    double calculate(SIUnit to);
    ~NumericFormulaLogicalAnd();
};

struct NumericFormulaLogicalOr : public NumericFormulaPair
{
    NumericFormulaLogicalOr(FormulaImplementation *f, SIUnit siu,
                             std::unique_ptr<NumericFormula> &a,
                             std::unique_ptr<NumericFormula> &b)
        : NumericFormulaPair(f, siu, a, b, "LOR", "||") {}
    double calculate(SIUnit to);
    ~NumericFormulaLogicalOr();
};

enum class TokenType
{
    SPACE,
    LPAR,
    RPAR,
    NUMBER,
    DATETIME,
    TIME,
    PLUS,
    MINUS,
    TIMES,
    DIV,
    MOD,
    SHL,
    SHR,
    EQ,
    NEQ,
    LT,
    GT,
    LTE,
    GTE,
    BAND,
    BOR,
    LAND,
    LOR,
    EXP,
    SQRT,
    ROUND,
    FLOOR,
    CEIL,
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

    std::string str(const std::string &s);
    std::string vals(const std::string &s);
    double val(const std::string &s);
    Unit unit(const std::string &s);

    std::string withMarker(const std::string &s);
};

struct FormulaImplementation : public Formula
{
    bool parse(Meter *m, const std::string &f);
    bool valid();
    std::string errors();
    double calculate(Unit to, DVEntry *dve = NULL, Meter *m = NULL);
    void clear();
    std::string str();
    std::string tree();
    SIUnit &siUnit();
    void setMeter(Meter *m);
    void setDVEntry(DVEntry *dve);

    // Pushes a constant on the formula builder stack.
    void doConstant(Unit u, double c);
    // Pushes a meter field read on the formula builder stack.
    void doMeterField(Unit u, FieldInfo *fi);
    // Pushes a dve entry field read on the formula builder stack.
    void doDVEntryField(Unit u, DVEntryCounterType ct);
    // Pops the two top nodes of the formula builder stack and pushes an addition on the formula builder stack.
    // The target unit will be the supplied to unit.
    void doAddition(const SIUnit &to);
    // Pops the two top nodes of the formula builder stack and pushes a subtraction on the formula builder stack.
    // The target unit will be the supplied to unit.
    void doSubtraction(const SIUnit &to);
    // Pops the two top nodes of the formula builder stack and pushes a multiplication on the formula builder stack.
    // The target unit will be multiplication of the SI Units.
    void doMultiplication();
    // Pops the two top nodes of the formula builder stack and pushes a division on the formula builder stack.
    // The target unit will be first SIUnit divided by the second SIUnit.
    void doDivision();
    // Pops the two top nodes of the formula builder stack and pushes an exponentiation on the formula builder stack.
    // The target unit will be first SIUnit exponentiated.
    void doExponentiation();
    // Pops the single top node of the formula builder stack and pushes an squareroot on the formula builder stack.
    // The target unit will be SIUnit square rooted.
    void doSquareRoot();
    void doRound();
    void doFloor();
    void doCeil();
    void doModulo();
    void doShiftLeft();
    void doShiftRight();
    void doEQ();
    void doNEQ();
    void doLT();
    void doGT();
    void doLTE();
    void doGTE();
    void doBitwiseAnd();
    void doBitwiseOr();
    void doLogicalAnd();
    void doLogicalOr();

    ~FormulaImplementation();

    bool tokenize();
    bool go();
    size_t findSpace(size_t i);
    size_t findNumber(size_t i);
    size_t findDateTime(size_t i);
    size_t findTime(size_t i);
    size_t findUnit(size_t i);
    size_t findPlus(size_t i);
    size_t findMinus(size_t i);
    size_t findTimes(size_t i);
    size_t findDiv(size_t i);
    size_t findMod(size_t i);
    size_t findShl(size_t i);
    size_t findShr(size_t i);
    size_t findEQ(size_t i);
    size_t findNEQ(size_t i);
    size_t findLT(size_t i);
    size_t findGT(size_t i);
    size_t findLTE(size_t i);
    size_t findGTE(size_t i);
    size_t findBitwiseAnd(size_t i);
    size_t findBitwiseOr(size_t i);
    size_t findLogicalAnd(size_t i);
    size_t findLogicalOr(size_t i);
    size_t findExp(size_t i);
    size_t findSqrt(size_t i);
    size_t findRound(size_t i);
    size_t findFloor(size_t i);
    size_t findCeil(size_t i);
    size_t findLPar(size_t i);
    size_t findRPar(size_t i);
    size_t findField(size_t i);

    Token *LA(size_t i);
    size_t parseOps(size_t i);
    size_t parsePar(size_t i);

    void handleConstant(Token *number, Token *unit);
    void handleSeconds(Token *number);
    void handleUnixTimestamp(Token *number);
    void handleAddition(Token *add);
    void handleSubtraction(Token *add);
    void handleMultiplication(Token *add);
    void handleDivision(Token *add);
    void handleModulo(Token *add);
    void handleShiftLeft(Token *add);
    void handleShiftRight(Token *add);
    void handleEQ(Token *tok);
    void handleNEQ(Token *tok);
    void handleLT(Token *tok);
    void handleGT(Token *tok);
    void handleLTE(Token *tok);
    void handleGTE(Token *tok);
    void handleBitwiseAnd(Token *tok);
    void handleBitwiseOr(Token *tok);
    void handleLogicalAnd(Token *tok);
    void handleLogicalOr(Token *tok);
    void handleExponentiation(Token *add);
    void handleSquareRoot(Token *add);
    void handleRound(Token *add);
    void handleFloor(Token *add);
    void handleCeil(Token *add);
    void handleField(Token *field);

    void pushOp(NumericFormula *nf);
    std::unique_ptr<NumericFormula> popOp();
    NumericFormula *topOp();
    NumericFormula *top2Op();

    Meter *meter() { return meter_; }
    DVEntry *dventry() { return dventry_; }

private:

    bool valid_ = true;
    std::vector<std::unique_ptr<NumericFormula>> op_stack_;
    std::vector<Token> tokens_;
    std::string formula_; // To be parsed.
    Meter *meter_; // To be referenced when parsing and calculating.
    DVEntry *dventry_; // To be referenced when calculating.

    // Any errors during parsing are store here.
    std::vector<std::string> errors_;
};

struct StringInterpolatorImplementation : public StringInterpolator
{
    // Create a string interpolation from for example: "historic_{storage_counter / - 12 counter}_value"
    // Which for a dventry with storage 13 will "generate historic_1_value"
    bool parse(Meter *m, const std::string &f);
    std::string apply(Meter *m, DVEntry *dve);
    ~StringInterpolatorImplementation();

    // The strings store "historic_" "_value"
    std::vector<std::string> strings_;
    // The formula stores the parsed "storage_counter / - 12 counter" formula.
    std::vector<std::unique_ptr<Formula>> formulas_;
};

#endif

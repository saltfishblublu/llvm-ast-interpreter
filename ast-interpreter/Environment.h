#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <stdio.h>
#include <iostream>
#include <map>
#include <vector>
#include <cassert>
#include <cstdlib>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;
using namespace std;

/**
 * StackFrame represents a function call frame in the execution stack
 * Manages variable declarations and expression evaluations within a scope
 */
class StackFrame
{
private:
    std::map<Decl *, int64_t> variableValues;
    std::map<Stmt *, int64_t> expressionValues;
    Stmt *currentStatement;
    bool hasReturnValue;
    int64_t returnValue;

public:
    StackFrame() : variableValues(), expressionValues(), currentStatement(nullptr), 
                   hasReturnValue(false), returnValue(0) {}

    // Return value management
    void setReturnInfo(bool hasValue, int64_t value)
    {
        hasReturnValue = hasValue;
        returnValue = value;
    }

    bool hasPendingReturn()
    {
        return hasReturnValue;
    }

    int64_t getReturnValue()
    {
        return returnValue;
    }

    // Variable management
    void bindVariable(Decl *decl, int64_t value) { 
        variableValues[decl] = value; 
    }
    
    int64_t getVariableValue(Decl *decl)
    {
        auto it = variableValues.find(decl);
        if (it != variableValues.end()) {
            return it->second;
        }
        return 0;
    }

    // Expression management  
    void bindExpression(Stmt *stmt, int64_t value) { 
        expressionValues[stmt] = value; 
    }
    
    int64_t getExpressionValue(Stmt *stmt)
    {
        auto it = expressionValues.find(stmt);
        if (it != expressionValues.end()) {
            return it->second;
        }
        return 0;
    }

    void storeExpressionResult(Stmt *stmt, int64_t value)
    {
        expressionValues[stmt] = value;
    }

    bool isExpressionEvaluated(Stmt *stmt)
    {
        return expressionValues.find(stmt) != expressionValues.end();
    }

    // Program counter management
    void setCurrentStatement(Stmt *stmt) { currentStatement = stmt; }
    Stmt *getCurrentStatement() { return currentStatement; }
};

/**
 * Main execution environment managing the interpreter state
 */
class ExecutionEnvironment
{
private:
    FunctionDecl *freeFunction;
    FunctionDecl *mallocFunction; 
    FunctionDecl *inputFunction;
    FunctionDecl *outputFunction;
    FunctionDecl *entryFunction;

    // Expression evaluation helper methods
    int64_t evaluateDeclRefExpr(DeclRefExpr *declRef)
    {
        processDeclRefExpr(declRef);
        return callStack.back().getExpressionValue(declRef);
    }

    int64_t evaluateIntegerLiteral(IntegerLiteral *intLiteral)
    {
        llvm::APInt value = intLiteral->getValue();
        return value.getSExtValue();
    }

    int64_t evaluateCharLiteral(CharacterLiteral *charLiteral)
    {
        return charLiteral->getValue();
    }

    int64_t evaluateUnaryExpr(UnaryOperator *unaryExpr)
    {
        evaluateUnaryOperation(unaryExpr);
        return callStack.back().getExpressionValue(unaryExpr);
    }

    int64_t evaluateBinaryExpr(BinaryOperator *binaryExpr)
    {
        evaluateBinaryOperation(binaryExpr);
        return callStack.back().getExpressionValue(binaryExpr);
    }

    int64_t evaluateArrayAccess(ArraySubscriptExpr *arrayAccess)
    {
        if (DeclRefExpr *declRef = dyn_cast<DeclRefExpr>(arrayAccess->getLHS()->IgnoreImpCasts()))
        {
            Decl *decl = declRef->getFoundDecl();
            int64_t index = evaluateExpression(arrayAccess->getRHS());
            
            if (VarDecl *varDecl = dyn_cast<VarDecl>(decl))
            {
                if (auto arrayType = dyn_cast<ConstantArrayType>(varDecl->getType().getTypePtr()))
                {
                    return accessArrayElement(varDecl, const_cast<ConstantArrayType*>(arrayType), index);
                }
            }
        }
        return 0;
    }

    int64_t accessArrayElement(VarDecl *varDecl, ConstantArrayType *arrayType, int64_t index)
    {
        int64_t baseAddress = callStack.back().getVariableValue(varDecl);
        
        if (arrayType->getElementType().getTypePtr()->isIntegerType())
        {
            int *arrayPtr = (int *)baseAddress;
            return *(arrayPtr + index);
        }
        else if (arrayType->getElementType().getTypePtr()->isCharType())
        {
            char *arrayPtr = (char *)baseAddress;
            return *(arrayPtr + index);
        }
        else
        {
            int64_t **arrayPtr = (int64_t **)baseAddress;
            return (int64_t)(*(arrayPtr + index));
        }
    }

    int64_t evaluateSizeOf(UnaryExprOrTypeTraitExpr *sizeofExpr)
    {
        if (sizeofExpr->getKind() == UETT_SizeOf)
        {
            if (sizeofExpr->getArgumentType()->isIntegerType())
            {
                return sizeof(int64_t);
            }
            else if (sizeofExpr->getArgumentType()->isPointerType())
            {
                return sizeof(int64_t *);
            }
        }
        return 0;
    }

    // Assignment handlers
    void handleAssignment(Expr *leftExpr, Expr *rightExpr)
    {
        if (DeclRefExpr *declRef = dyn_cast<DeclRefExpr>(leftExpr))
        {
            int64_t value = evaluateExpression(rightExpr);
            callStack.back().bindExpression(leftExpr, value);
            Decl *decl = declRef->getFoundDecl();
            callStack.back().bindVariable(decl, value);
        }
        else if (auto arrayAccess = dyn_cast<ArraySubscriptExpr>(leftExpr))
        {
            handleArrayAssignment(arrayAccess, rightExpr);
        }
        else if (auto unaryExpr = dyn_cast<UnaryOperator>(leftExpr))
        {
            handlePointerAssignment(unaryExpr, rightExpr);
        }
    }

    void handleArrayAssignment(ArraySubscriptExpr *arrayAccess, Expr *rightExpr)
    {
        if (DeclRefExpr *declRef = dyn_cast<DeclRefExpr>(arrayAccess->getLHS()->IgnoreImpCasts()))
        {
            Decl *decl = declRef->getFoundDecl();
            int64_t value = evaluateExpression(rightExpr);
            int64_t index = evaluateExpression(arrayAccess->getRHS());
            
            if (VarDecl *varDecl = dyn_cast<VarDecl>(decl))
            {
                // 修复：使用 const_cast 来移除 const 限定符
                if (auto arrayType = dyn_cast<ConstantArrayType>(varDecl->getType().getTypePtr()))
                {
                    updateArrayElement(varDecl, const_cast<ConstantArrayType*>(arrayType), index, value);
                }
            }
        }
    }

    void updateArrayElement(VarDecl *varDecl, ConstantArrayType *arrayType, int64_t index, int64_t value)
    {
        int64_t baseAddress = callStack.back().getVariableValue(varDecl);
        
        if (arrayType->getElementType().getTypePtr()->isIntegerType())
        {
            int *arrayPtr = (int *)baseAddress;
            *(arrayPtr + index) = value;
        }
        else if (arrayType->getElementType().getTypePtr()->isCharType())
        {
            char *arrayPtr = (char *)baseAddress;
            *(arrayPtr + index) = (char)value;
        }
        else
        {
            int64_t **arrayPtr = (int64_t **)baseAddress;
            *(arrayPtr + index) = (int64_t *)value;
        }
    }

    void handlePointerAssignment(UnaryOperator *unaryExpr, Expr *rightExpr)
    {
        int64_t value = evaluateExpression(rightExpr);
        int64_t address = evaluateExpression(unaryExpr->getSubExpr());
        int64_t *pointer = (int64_t *)address;
        *pointer = value;
    }

    // Binary operation handlers
    int64_t handleAddition(Expr *leftExpr, Expr *rightExpr)
    {
        if (leftExpr->getType().getTypePtr()->isPointerType())
        {
            int64_t baseAddress = evaluateExpression(leftExpr);
            return baseAddress + sizeof(int64_t) * evaluateExpression(rightExpr);
        }
        else
        {
            return evaluateExpression(leftExpr) + evaluateExpression(rightExpr);
        }
    }

    int64_t handleDivision(Expr *leftExpr, Expr *rightExpr)
    {
        int64_t divisor = evaluateExpression(rightExpr);
        if (divisor == 0)
        {
            cerr << "Division by zero error" << endl;
            exit(1);
        }
        
        int64_t dividend = evaluateExpression(leftExpr);
        return int64_t(dividend / divisor);
    }

    // Array initialization
    void initializeArrayVariable(VarDecl *varDecl)
    {
        // 修复：使用 const_cast 来移除 const 限定符
        if (auto arrayType = dyn_cast<ConstantArrayType>(varDecl->getType().getTypePtr()))
        {
            int64_t arraySize = arrayType->getSize().getSExtValue();
            
            if (arrayType->getElementType().getTypePtr()->isIntegerType())
            {
                int *array = new int[arraySize];
                initializeArray(array, arraySize);
                callStack.back().bindVariable(varDecl, (int64_t)array);
            }
            else if (arrayType->getElementType().getTypePtr()->isCharType())
            {
                char *array = new char[arraySize];
                initializeArray(array, arraySize);
                callStack.back().bindVariable(varDecl, (int64_t)array);
            }
            else
            {
                int64_t **array = new int64_t *[arraySize];
                initializeArray(array, arraySize);
                callStack.back().bindVariable(varDecl, (int64_t)array);
            }
        }
    }

    template<typename T>
    void initializeArray(T *array, int64_t size)
    {
        for (int i = 0; i < size; i++)
        {
            array[i] = 0;
        }
    }

    // Function call handlers
    void handleInputCall(CallExpr *callExpr)
    {
        int64_t inputValue;
        cout << "Please Input an Integer Value: " << endl;
        cin >> inputValue;
        callStack.back().storeExpressionResult(callExpr, inputValue);
    }

    void handleOutputCall(CallExpr *callExpr)
    {
        Expr *arg = callExpr->getArg(0);
        int64_t value = evaluateExpression(arg);
        cout << value << endl;
    }

    void handleMallocCall(CallExpr *callExpr)
    {
        int64_t size = evaluateExpression(callExpr->getArg(0));
        int64_t *memory = (int64_t *)std::malloc(size);
        callStack.back().storeExpressionResult(callExpr, (int64_t)memory);
    }

    void handleFreeCall(CallExpr *callExpr)
    {
        int64_t *memory = (int64_t *)evaluateExpression(callExpr->getArg(0));
        std::free(memory);
    }

    void handleUserFunctionCall(CallExpr *callExpr, FunctionDecl *callee)
    {
        vector<int64_t> arguments;
        for (auto i = callExpr->arg_begin(), e = callExpr->arg_end(); i != e; i++)
        {
            arguments.push_back(evaluateExpression(*i));
        }
        
        callStack.push_back(StackFrame());
        
        int paramIndex = 0;
        for (auto i = callee->param_begin(), e = callee->param_end(); i != e; i++, paramIndex++)
        {
            callStack.back().bindVariable(*i, arguments[paramIndex]);
        }
    }

public:
    std::vector<StackFrame> callStack;

    // 修复：按照声明顺序初始化成员变量
    ExecutionEnvironment() : freeFunction(nullptr), mallocFunction(nullptr), 
                            inputFunction(nullptr), outputFunction(nullptr), 
                            entryFunction(nullptr), callStack() {}

    /**
     * Initializes the execution environment with translation unit
     */
    void initialize(TranslationUnitDecl *unit)
    {
        callStack.push_back(StackFrame());
        
        for (TranslationUnitDecl::decl_iterator i = unit->decls_begin(), e = unit->decls_end(); i != e; ++i)
        {
            if (FunctionDecl *funcDecl = dyn_cast<FunctionDecl>(*i))
            {
                if (funcDecl->getName() == "FREE")
                    freeFunction = funcDecl;
                else if (funcDecl->getName() == "MALLOC")
                    mallocFunction = funcDecl;
                else if (funcDecl->getName() == "GET")
                    inputFunction = funcDecl;
                else if (funcDecl->getName() == "PRINT")
                    outputFunction = funcDecl;
                else if (funcDecl->getName() == "main")
                    entryFunction = funcDecl;
            }
            else if (VarDecl *varDecl = dyn_cast<VarDecl>(*i))
            {
                if (varDecl->getType().getTypePtr()->isIntegerType() || 
                    varDecl->getType().getTypePtr()->isCharType() ||
                    varDecl->getType().getTypePtr()->isPointerType())
                {
                    if (varDecl->hasInit())
                        callStack.back().bindVariable(varDecl, evaluateExpression(varDecl->getInit()));
                    else
                        callStack.back().bindVariable(varDecl, 0);
                }
            }
        }
    }

    FunctionDecl *getEntryFunction() { return entryFunction; }

    /**
     * Evaluates binary operations including arithmetic and comparisons
     */
    void evaluateBinaryOperation(BinaryOperator *binaryExpr)
    {
        Expr *leftExpr = binaryExpr->getLHS();
        Expr *rightExpr = binaryExpr->getRHS();
        
        if (binaryExpr->isAssignmentOp())
        {
            handleAssignment(leftExpr, rightExpr);
        }
        else
        {
            evaluateBinaryOperator(binaryExpr, leftExpr, rightExpr);
        }
    }

    void evaluateBinaryOperator(BinaryOperator *binaryExpr, Expr *leftExpr, Expr *rightExpr)
    {
        auto operatorCode = binaryExpr->getOpcode();
        int64_t result = 0;
        
        switch (operatorCode)
        {
            case BO_Add:
                result = handleAddition(leftExpr, rightExpr);
                break;
            case BO_Sub:
                result = evaluateExpression(leftExpr) - evaluateExpression(rightExpr);
                break;
            case BO_Mul:
                result = evaluateExpression(leftExpr) * evaluateExpression(rightExpr);
                break;
            case BO_Div:
                result = handleDivision(leftExpr, rightExpr);
                break;
            case BO_LT:
                result = evaluateExpression(leftExpr) < evaluateExpression(rightExpr);
                break;
            case BO_GT:
                result = evaluateExpression(leftExpr) > evaluateExpression(rightExpr);
                break;
            case BO_EQ:
                result = evaluateExpression(leftExpr) == evaluateExpression(rightExpr);
                break;
            case BO_LE:
                result = evaluateExpression(leftExpr) <= evaluateExpression(rightExpr);
                break;
            case BO_GE:
                result = evaluateExpression(leftExpr) >= evaluateExpression(rightExpr);
                break;
            case BO_NE:
                result = evaluateExpression(leftExpr) != evaluateExpression(rightExpr);
                break;
            default:
                cerr << "Unsupported binary operator" << endl;
                exit(1);
        }
        
        callStack.back().storeExpressionResult(binaryExpr, result);
    }

    /**
     * Processes variable declaration statements
     */
    void processDeclaration(DeclStmt *declStmt)
    {
        for (DeclStmt::decl_iterator it = declStmt->decl_begin(), ie = declStmt->decl_end();
             it != ie; ++it)
        {
            Decl *decl = *it;
            if (VarDecl *varDecl = dyn_cast<VarDecl>(decl))
            {
                if (varDecl->getType().getTypePtr()->isIntegerType() || 
                    varDecl->getType().getTypePtr()->isPointerType())
                {
                    if (varDecl->hasInit())
                        callStack.back().bindVariable(varDecl, evaluateExpression(varDecl->getInit()));
                    else
                        callStack.back().bindVariable(varDecl, 0);
                }
                else
                {
                    initializeArrayVariable(varDecl);
                }
            }
        }
    }

    /**
     * Processes return statements
     */
    void processReturn(ReturnStmt *returnStmt)
    {
        int64_t returnValue = evaluateExpression(returnStmt->getRetValue());
        callStack.back().setReturnInfo(true, returnValue);
    }

    /**
     * Evaluates unary operations
     */
    void evaluateUnaryOperation(UnaryOperator *unaryExpr)
    {
        auto operatorCode = unaryExpr->getOpcode();
        auto subExpr = unaryExpr->getSubExpr();

        if (subExpr == nullptr)
        {
            cerr << "Error: null pointer dereference" << endl;
            return;
        }

        int64_t result = 0;
        switch (operatorCode)
        {
            case UO_Minus:
                result = -1 * evaluateExpression(subExpr);
                break;
            case UO_Plus:
                result = evaluateExpression(subExpr);
                break;
            case UO_Deref:
                result = *(int64_t *)evaluateExpression(unaryExpr->getSubExpr());
                break;
            default:
                cerr << "Unsupported unary operator" << endl;
                exit(1);
        }
        
        callStack.back().storeExpressionResult(unaryExpr, result);
    }

    /**
     * Main expression evaluation dispatcher
     */
    int64_t evaluateExpression(Expr *expr)
    {
        expr = expr->IgnoreImpCasts();
        
        if (auto declRef = dyn_cast<DeclRefExpr>(expr))
        {
            return evaluateDeclRefExpr(declRef);
        }
        else if (auto intLiteral = dyn_cast<IntegerLiteral>(expr))
        {
            return evaluateIntegerLiteral(intLiteral);
        }
        else if (auto charLiteral = dyn_cast<CharacterLiteral>(expr))
        {
            return evaluateCharLiteral(charLiteral);
        }
        else if (auto unaryExpr = dyn_cast<UnaryOperator>(expr))
        {
            return evaluateUnaryExpr(unaryExpr);
        }
        else if (auto binaryExpr = dyn_cast<BinaryOperator>(expr))
        {
            return evaluateBinaryExpr(binaryExpr);
        }
        else if (auto parenExpr = dyn_cast<ParenExpr>(expr))
        {
            return evaluateExpression(parenExpr->getSubExpr());
        }
        else if (auto arrayAccess = dyn_cast<ArraySubscriptExpr>(expr))
        {
            return evaluateArrayAccess(arrayAccess);
        }
        else if (auto callExpr = dyn_cast<CallExpr>(expr))
        {
            return callStack.back().getExpressionValue(callExpr);
        }
        else if (auto sizeofExpr = dyn_cast<UnaryExprOrTypeTraitExpr>(expr))
        {
            return evaluateSizeOf(sizeofExpr);
        }
        else if (auto castExpr = dyn_cast<CStyleCastExpr>(expr))
        {
            return evaluateExpression(castExpr->getSubExpr());
        }
        
        cerr << "Unhandled expression type" << endl;
        return 0;
    }

    /**
     * Processes declaration reference expressions
     */
    void processDeclRefExpr(DeclRefExpr *declRef)
    {
        callStack.back().setCurrentStatement(declRef);
        if (declRef->getType()->isIntegerType() || declRef->getType()->isPointerType())
        {
            Decl *decl = declRef->getFoundDecl();
            int64_t value = callStack.back().getVariableValue(decl);
            callStack.back().bindExpression(declRef, value);
        }
    }

    /**
     * Processes function call expressions
     */
    void processFunctionCall(CallExpr *callExpr)
    {
        callStack.back().setCurrentStatement(callExpr);
        FunctionDecl *callee = callExpr->getDirectCallee();
        
        if (callee == inputFunction)
        {
            handleInputCall(callExpr);
        }
        else if (callee == outputFunction)
        {
            handleOutputCall(callExpr);
        }
        else if (callee == mallocFunction)
        {
            handleMallocCall(callExpr);
        }
        else if (callee == freeFunction)
        {
            handleFreeCall(callExpr);
        }
        else
        {
            handleUserFunctionCall(callExpr, callee);
        }
    }
};

#endif // ENVIRONMENT_H

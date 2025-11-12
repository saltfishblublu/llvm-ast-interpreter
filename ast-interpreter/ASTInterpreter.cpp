//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

#include "Environment.h"

/**
 * AST Visitor that traverses and interprets the abstract syntax tree
 */
class ASTInterpreterVisitor : public EvaluatedExprVisitor<ASTInterpreterVisitor>
{
private:
    ExecutionEnvironment *executionEnv;

    /**
     * Checks if execution should be skipped due to pending return
     */
    bool shouldSkipExecution()
    {
        return executionEnv->callStack.back().hasPendingReturn();
    }

    /**
     * Checks if function is user-defined (not built-in)
     */
    bool isUserDefinedFunction(FunctionDecl *funcDecl)
    {
        return (funcDecl->getName() != "GET") &&
               (funcDecl->getName() != "PRINT") &&
               (funcDecl->getName() != "MALLOC") &&
               (funcDecl->getName() != "FREE");
    }

public:
    explicit ASTInterpreterVisitor(const ASTContext &context, ExecutionEnvironment *env)
        : EvaluatedExprVisitor(context), executionEnv(env) {}
        
    virtual ~ASTInterpreterVisitor() {}

    /**
     * Visits binary operator expressions
     */
    virtual void VisitBinaryOperator(BinaryOperator *binaryOp)
    {
        if (shouldSkipExecution()) return;
        
        VisitStmt(binaryOp);
        executionEnv->evaluateBinaryOperation(binaryOp);
    }

    /**
     * Visits declaration reference expressions
     */
    virtual void VisitDeclRefExpr(DeclRefExpr *declRef)
    {
        if (shouldSkipExecution()) return;
        
        VisitStmt(declRef);
        executionEnv->processDeclRefExpr(declRef);
    }

    /**
     * Visits function call expressions
     */
    virtual void VisitCallExpr(CallExpr *callExpr)
    {
        if (shouldSkipExecution()) return;
        
        VisitStmt(callExpr);
        executionEnv->processFunctionCall(callExpr);
        
        // Handle user-defined function returns
        if (FunctionDecl *callee = callExpr->getDirectCallee())
        {
            if (isUserDefinedFunction(callee))
            {
                // Visit function body
                Visit(callee->getBody());
                
                // Retrieve return value and clean up stack frame
                int64_t returnValue = executionEnv->callStack.back().getReturnValue();
                executionEnv->callStack.pop_back();
                executionEnv->callStack.back().storeExpressionResult(callExpr, returnValue);
            }
        }
    }

    /**
     * Visits declaration statements
     */
    virtual void VisitDeclStmt(DeclStmt *declStmt)
    {
        if (shouldSkipExecution()) return;
        
        VisitStmt(declStmt);
        executionEnv->processDeclaration(declStmt);
    }

    /**
     * Visits if statements
     */
    virtual void VisitIfStmt(IfStmt *ifStmt)
    {
        if (shouldSkipExecution()) return;
        
        Expr *condition = ifStmt->getCond();
        if (executionEnv->evaluateExpression(condition))
        {
            // Execute then branch
            Stmt *thenBranch = ifStmt->getThen();
            Visit(thenBranch);
        }
        else if (ifStmt->getElse())
        {
            // Execute else branch
            Stmt *elseBranch = ifStmt->getElse();
            Visit(elseBranch);
        }
    }

    /**
     * Visits while statements
     */
    virtual void VisitWhileStmt(WhileStmt *whileStmt)
    {
        if (shouldSkipExecution()) return;
        
        Expr *condition = whileStmt->getCond();
        while (executionEnv->evaluateExpression(condition))
        {
            Visit(whileStmt->getBody());
        }
    }

    /**
     * Visits for statements
     */
    virtual void VisitForStmt(ForStmt *forStmt)
    {
        if (shouldSkipExecution()) return;
        
        // Execute initialization
        Stmt *initialization = forStmt->getInit();
        if (initialization)
        {
            Visit(initialization);
        }
        
        // Execute loop
        for (; executionEnv->evaluateExpression(forStmt->getCond()); 
             Visit(forStmt->getInc()))
        {
            Visit(forStmt->getBody());
        }
    }

    /**
     * Visits return statements
     */
    virtual void VisitReturnStmt(ReturnStmt *returnStmt)
    {
        if (shouldSkipExecution()) return;
        
        Visit(returnStmt->getRetValue());
        executionEnv->processReturn(returnStmt);
    }

    /**
     * Visits unary operator expressions
     */
    virtual void VisitUnaryOperator(UnaryOperator *unaryOp)
    {
        if (shouldSkipExecution()) return;
        
        VisitStmt(unaryOp);
        executionEnv->evaluateUnaryOperation(unaryOp);
    }
};

/**
 * AST Consumer that handles the entire translation unit
 */
class InterpreterConsumer : public ASTConsumer
{
private:
    ExecutionEnvironment executionEnv;
    ASTInterpreterVisitor astVisitor;

public:
    explicit InterpreterConsumer(const ASTContext &context) 
        : executionEnv(), astVisitor(context, &executionEnv) {}
        
    virtual ~InterpreterConsumer() {}

    /**
     * Handles the complete translation unit
     */
    virtual void HandleTranslationUnit(clang::ASTContext &context)
    {
        TranslationUnitDecl *translationUnit = context.getTranslationUnitDecl();
        executionEnv.initialize(translationUnit);

        // Execute main function
        FunctionDecl *mainFunction = executionEnv.getEntryFunction();
        if (mainFunction) {
            astVisitor.VisitStmt(mainFunction->getBody());
        }
    }
};

/**
 * Frontend action that creates the interpreter consumer
 */
class InterpreterFrontendAction : public ASTFrontendAction
{
public:
    virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance &compiler, llvm::StringRef inputFile)
    {
        return std::unique_ptr<clang::ASTConsumer>(
            new InterpreterConsumer(compiler.getASTContext()));
    }
};

/**
 * Main entry point for the AST interpreter
 */
int main(int argc, char **argv)
{
    if (argc > 1)
    {
        clang::tooling::runToolOnCode(
            std::make_unique<InterpreterFrontendAction>(), argv[1], "input.c");
    }
    
    return 0;
}


#include "Parselet.h"

#include "SourceManager.h"
#include "Symbol.h"
#include "Utils.h"

//
// Symbol parselet
//

//
// parsing x in _x
//
// we know it can only be a symbol
//
// Called from other parselets
//
NodePtr SymbolParselet::parseContextSensitive(ParserContext CtxtIn) const {
    
    auto TokIn = TheParser->currentToken();
    
    auto Ctxt = CtxtIn;
    
    TheParser->nextToken(Ctxt);
    
    return std::unique_ptr<Node>(new LeafNode(TokIn));
}

//
// something like  x  or x_
//
NodePtr SymbolParselet::parse(ParserContext CtxtIn) const {
    
    auto TokIn = TheParser->currentToken();
    
    auto Sym = std::unique_ptr<Node>(new LeafNode(TokIn));
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1);
    
    auto Ctxt = CtxtIn;
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    //
    // if we are here, then we know that Sym could bind to _
    //
    
    if (Tok.Tok == TOKEN_UNDER) {
        
        auto& underParselet = TheParser->findContextSensitiveInfixParselet(Tok.Tok);
        
        Ctxt.UnderCount = UNDER_1;
        
        Args->append(std::move(Sym));
        
        return underParselet->parseContextSensitive(std::move(Args), Ctxt);
        
    } else if (Tok.Tok == TOKEN_UNDERUNDER) {
        
        auto& underParselet = TheParser->findContextSensitiveInfixParselet(Tok.Tok);
        
        Ctxt.UnderCount = UNDER_2;
        
        Args->append(std::move(Sym));
        
        return underParselet->parseContextSensitive(std::move(Args), Ctxt);
        
    } else if (Tok.Tok == TOKEN_UNDERUNDERUNDER) {
        
        auto& underParselet = TheParser->findContextSensitiveInfixParselet(Tok.Tok);
        
        Ctxt.UnderCount = UNDER_3;
        
        Args->append(std::move(Sym));
        
        return underParselet->parseContextSensitive(std::move(Args), Ctxt);
        
    } else if (Tok.Tok == TOKEN_UNDERDOT) {
        
        Args->append(std::move(Sym));
        
        Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
        
        TheParser->nextToken(Ctxt);
        
        return std::unique_ptr<Node>(new OptionalDefaultPatternNode(std::move(Args)));
    }
    
    //
    // LOOKAHEAD
    //
    {
        auto ArgsTest = std::unique_ptr<LeafSeq>(new LeafSeq);
        
        Tok = Parser::eatAll(Tok, Ctxt, ArgsTest);
        
        //
        // when parsing a in a:b  then ColonFlag is false
        // when parsing b in a:b  then ColonFlag is true
        //
        // It is necessary to go to colonParselet->parse here (even though it seems non-contextSensitive)
        // because in e.g.,   a_*b:f[]    the b is the last node in the Times expression and needs to bind with :f[]
        // Parsing a_*b completely, and then parsing :f[] would be wrong.
        //
        if ((Ctxt.Flag & PARSER_COLON) != PARSER_COLON) {
            
            if (Tok.Tok == TOKEN_COLON) {
                
                Ctxt.Flag |= PARSER_PARSED_SYMBOL;
                
                Args->append(std::move(Sym));
                Args->append(std::move(ArgsTest));
                
                auto& colonParselet = TheParser->findInfixParselet(Tok.Tok);
                
                return colonParselet->parse(std::move(Args), Ctxt);
            }
        }
        
        //
        // Prepend in correct order
        //
        TheParser->prependInReverse(std::move(ArgsTest));
        
        return Sym;
    }
}


//
// Base Operators parselets
//

NodePtr PrefixOperatorParselet::parse(ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1);
    
    auto TokIn = TheParser->currentToken();
    
    Args->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = getPrecedence();
    Ctxt.Assoc = ASSOCIATIVITY_NONE;
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    Tok = Parser::eatAll(Tok, Ctxt, Args);
    
    Utils::differentLineWarning(TokIn, Tok, SYNTAXISSUESEVERITY_FORMATTING);
    
    auto operand = TheParser->parse(Ctxt);
    
    Args->append(std::move(operand));
    
    return std::unique_ptr<Node>(new PrefixNode(PrefixOperatorToSymbol(TokIn.Tok), std::move(Args)));
}

NodePtr BinaryOperatorParselet::parse(std::unique_ptr<NodeSeq> Left, ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1 + 1);
    
    auto TokIn = TheParser->currentToken();
    
    Args->append(std::move(Left));
    
    Args->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = getPrecedence();
    Ctxt.Assoc = getAssociativity();
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    Tok = Parser::eatAll(Tok, Ctxt, Args);
    
    auto Right = TheParser->parse(Ctxt);
    
    Args->append(std::move(Right));
    
    return std::unique_ptr<Node>(new BinaryNode(BinaryOperatorToSymbol(TokIn.Tok), std::move(Args)));
}

NodePtr InfixOperatorParselet::parse(std::unique_ptr<NodeSeq> Left, ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1 + 1);
    
    Args->append(std::move(Left));
    
    auto TokIn = TheParser->currentToken();
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = getPrecedence();
    Ctxt.Assoc = ASSOCIATIVITY_NONE;
    
    while (true) {
        
        //
        // Check isAbort() inside loops
        //
        if (TheParser->isAbort()) {
            
            auto A = Token(TOKEN_ERROR_ABORTED, "", Source(TheSourceManager->getSourceLocation()));
            
            auto Aborted = std::unique_ptr<Node>(new LeafNode(A));
            
            return Aborted;
        }
        
        
        //
        // LOOKAHEAD
        //
        {
            auto ArgsTest = std::unique_ptr<LeafSeq>(new LeafSeq);
            
            auto Tok = TheParser->currentToken();
            
            Tok = Parser::eatAndPreserveToplevelNewlines(Tok, CtxtIn, ArgsTest);
            
            //
            // Cannot just compare tokens
            //
            // May be something like  a * b c \[Times] d
            //
            // and we want only a single Infix node created
            //
            if (isInfixOperator(Tok.Tok) &&
                InfixOperatorToSymbol(Tok.Tok) == InfixOperatorToSymbol(TokIn.Tok)) {
                
                Args->append(std::move(ArgsTest));
                
                Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
                
                Tok = TheParser->nextToken(Ctxt);
                
                Tok = Parser::eatAll(Tok, Ctxt, Args);
                
                auto operand = TheParser->parse(Ctxt);
                
                Args->append(std::move(operand));
                
            } else {
                
                //
                // Tok.Tok != TokIn.Tok, so break
                //
                //
                // Prepend in correct order
                //
                TheParser->prependInReverse(std::move(ArgsTest));
                
                return std::unique_ptr<Node>(new InfixNode(InfixOperatorToSymbol(TokIn.Tok), std::move(Args)));
            }
        }
        
    } // while
}

NodePtr PostfixOperatorParselet::parse(std::unique_ptr<NodeSeq> Operand, ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1);
    
    auto TokIn = TheParser->currentToken();
    
    Utils::differentLineWarning(Operand, TokIn, SYNTAXISSUESEVERITY_FORMATTING);
    
    Args->append(std::move(Operand));
    
    Args->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
    
    
    auto Ctxt = CtxtIn;
    
    TheParser->nextToken(Ctxt);
    
    return std::unique_ptr<Node>(new PostfixNode(PostfixOperatorToSymbol(TokIn.Tok), std::move(Args)));
}




//
// Group parselets
//

NodePtr GroupParselet::parse(ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1 + 1);
    
    auto Opener = TheParser->currentToken();
    
    Args->append(std::unique_ptr<Node>(new LeafNode(Opener)));
    
    auto Ctxt = CtxtIn;
    Ctxt.GroupDepth++;
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    auto& Op = GroupOpenerToSymbol(Opener.Tok);
    
    auto CloserTok = GroupOpenerToCloser(Opener.Tok);
    Ctxt.Closer = CloserTok;
    
    //
    // There will only be 1 "good" node (either a LeafNode or a CommaNode)
    // But there might be multiple error nodes
    //
    while (true) {
        
        //
        // Check isAbort() inside loops
        //
        if (TheParser->isAbort()) {
            
            auto A = Token(TOKEN_ERROR_ABORTED, "", Source(TheSourceManager->getSourceLocation()));
            
            auto Aborted = std::unique_ptr<Node>(new LeafNode(A));
            
            return Aborted;
        }
        
        
        auto Tok = TheParser->currentToken();
        
        Tok = Parser::eatAll(Tok, Ctxt, Args);
        
        if (Tok.Tok == CloserTok) {
            
            //
            // Everything is good
            //
            
            Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
            
            auto Ctxt2 = Ctxt;
            Ctxt2.GroupDepth--;
            
            TheParser->nextToken(Ctxt2);
            
            auto group = std::unique_ptr<Node>(new GroupNode(Op, std::move(Args)));
            
            return group;
        }
        if (isCloser(Tok.Tok)) {
            
            //
            // some other closer
            //
            // e.g.,   { ( }
            //
            // FIXME: { ) }  is not handled
            //
            // Important to not duplicate token's Str here, it may also appear later
            //
            // Also, invent Source
            //
            
            auto createdToken = Token(TOKEN_ERROR_EXPECTEDOPERAND, "", Source(Tok.Src.start()));
            
            Args->append(std::unique_ptr<Node>(new LeafNode(createdToken)));
            
            auto group = std::unique_ptr<Node>(new GroupMissingCloserNode(Op, std::move(Args)));
            
            return group;
        }
        if (Tok.Tok == TOKEN_ENDOFFILE) {
            
            //
            // Handle something like   { a EOF
            //
            
            Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
            
            auto group = std::unique_ptr<Node>(new GroupMissingCloserNode(Op, std::move(Args)));
            
            return group;
        }
        
        //
        // Handle the expression
        //
        
        Tok = Parser::eatAll(Tok, Ctxt, Args);
        
        auto Ctxt2 = Ctxt;
        Ctxt2.Flag.clear(PARSER_COLON);
        Ctxt2.Prec = PRECEDENCE_LOWEST;
        Ctxt2.Assoc = ASSOCIATIVITY_NONE;
        Ctxt2.UnderCount = UNDER_UNKNOWN;
        
        auto operand = TheParser->parse(Ctxt2);
        
        Args->append(std::move(operand));
        
    } // while
}


//
// Call parselets
//

NodePtr CallParselet::parse(std::unique_ptr<NodeSeq> Head, ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1);
    
    auto TokIn = TheParser->currentToken();
    
    //
    // if we used PRECEDENCE_CALL here, then e.g., a[]?b should technically parse as   a <call> []?b
    //
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = PRECEDENCE_HIGHEST;
    Ctxt.Assoc = ASSOCIATIVITY_NONE;
    
    auto& groupParselet = TheParser->findPrefixParselet(TokIn.Tok);
    
    auto Right = groupParselet->parse(Ctxt);

#ifndef NDEBUG
    auto R = Right.get();
    
    assert(dynamic_cast<GroupNode*>(R) ||
           dynamic_cast<GroupMissingCloserNode*>(R) ||
           dynamic_cast<GroupMissingOpenerNode*>(R));
#endif
    
    Args->append(std::move(Right));
    
    return std::unique_ptr<Node>(new CallNode(std::move(Head), std::move(Args)));
}


//
// StartOfLine
//

NodePtr StartOfLineParselet::parse(ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1);
    
    auto TokIn = TheParser->currentToken();
    
    Args->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
    
    auto Ctxt = CtxtIn;
    Ctxt.Flag |= PARSER_STRINGIFY_CURRENT_LINE;
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    Ctxt.Flag.clear(PARSER_STRINGIFY_CURRENT_LINE);
    
    //
    // Cannot use TheParser->findPrefixParselet(TOKEN_STRING) here because TOKEN_ERROR_EMPTYSTRING
    // may be returned, and we have to handle that also
    //
    // So just use general parse
    //
    
    TheParser->nextToken(Ctxt);
    
    auto Operand = std::unique_ptr<Node>(new LeafNode(Tok));
    
    Args->append(std::move(Operand));
    
    return std::unique_ptr<Node>(new StartOfLineNode(StartOfLineOperatorToSymbol(TokIn.Tok), std::move(Args)));
}






//
// Special parselets
//

//
// prefix
//
// Something like  _a
//
NodePtr UnderParselet::parse(ParserContext CtxtIn) const {
    
    auto TokIn = TheParser->currentToken();
    
    auto Under = std::unique_ptr<Node>(new LeafNode(TokIn));
    
    auto Ctxt = CtxtIn;
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    std::unique_ptr<Node> Blank;
    if (Tok.Tok == TOKEN_SYMBOL) {
        
        auto& symbolParselet = TheParser->findContextSensitivePrefixParselet(Tok.Tok);
        
        auto Sym2 = symbolParselet->parseContextSensitive(Ctxt);
        
        auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
        Args->reserve(1 + 1);
        Args->append(std::move(Under));
        Args->append(std::move(Sym2));
        
        switch (TokIn.Tok) {
            case TOKEN_UNDER:
                Blank = std::unique_ptr<Node>(new BlankNode(std::move(Args)));
                break;
            case TOKEN_UNDERUNDER:
                Blank = std::unique_ptr<Node>(new BlankSequenceNode(std::move(Args)));
                break;
            case TOKEN_UNDERUNDERUNDER:
                Blank = std::unique_ptr<Node>(new BlankNullSequenceNode(std::move(Args)));
                break;
            default:
                assert(false);
                break;
        }
        
    } else {
        Blank = std::move(Under);
    }
    
    //
    // LOOKAHEAD
    //
    {
        auto ArgsTest = std::unique_ptr<LeafSeq>(new LeafSeq);
        
        Tok = TheParser->currentToken();
        
        Tok = Parser::eatAndPreserveToplevelNewlines(Tok, CtxtIn, ArgsTest);
        
        //
        // For something like _:""  when parsing _
        // ColonFlag == false
        // the : here is Optional, and so we want to go parse with ColonParselet's parseContextSensitive method
        //
        // For something like a:_:""  when parsing _
        // ColonFlag == true
        // make sure to not parse the second : here
        // We are already inside ColonParselet from the first :, and so ColonParselet will also handle the second :
        //
        if ((Ctxt.Flag & PARSER_COLON) != PARSER_COLON) {
            
            if (Tok.Tok == TOKEN_COLON) {
                
                auto& colonParselet = TheParser->findContextSensitiveInfixParselet(Tok.Tok);
                
                auto BlankSeq = std::unique_ptr<NodeSeq>(new NodeSeq);
                BlankSeq->reserve(1 + 1);
                BlankSeq->append(std::move(Blank));
                BlankSeq->append(std::move(ArgsTest));
                return colonParselet->parseContextSensitive(std::move(BlankSeq), Ctxt);
            }
        }
        
        //
        // Prepend in correct order
        //
        
        TheParser->prependInReverse(std::move(ArgsTest));
        
        return Blank;
    }
}

//
// infix
//
// Something like  a_b
//
// Called from other parselets
//
NodePtr UnderParselet::parseContextSensitive(std::unique_ptr<NodeSeq> Left, ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1);
    
    Args->append(std::move(Left));
    
    auto TokIn = TheParser->currentToken();
    
    Args->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
    
    auto UnderCount = CtxtIn.UnderCount;
    
    auto Ctxt = CtxtIn;
    Ctxt.UnderCount = UNDER_UNKNOWN;
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    if (Tok.Tok == TOKEN_SYMBOL) {
        
        auto& symbolParselet = TheParser->findContextSensitivePrefixParselet(Tok.Tok);
        
        auto Right = symbolParselet->parseContextSensitive(Ctxt);
        
        Args->append(std::move(Right));
    }
    
    std::unique_ptr<Node> Pat;
    switch (UnderCount) {
        case UNDER_1:
            Pat = std::unique_ptr<Node>(new PatternBlankNode(std::move(Args)));
            break;
        case UNDER_2:
            Pat = std::unique_ptr<Node>(new PatternBlankSequenceNode(std::move(Args)));
            break;
        case UNDER_3:
            Pat = std::unique_ptr<Node>(new PatternBlankNullSequenceNode(std::move(Args)));
            break;
        default:
            assert(false);
            break;
    }
    
    
    //
    // LOOKAHEAD
    //
    {
        auto ArgsTest = std::unique_ptr<LeafSeq>(new LeafSeq);
        
        Tok = TheParser->currentToken();
        
        Tok = Parser::eatAll(Tok, Ctxt, ArgsTest);
        
        //
        // For something like a:b_c:d when parsing _
        // ColonFlag == true
        //
        if ((Ctxt.Flag & PARSER_COLON) != PARSER_COLON) {
            
            if (Tok.Tok == TOKEN_COLON) {
                
                auto& colonParselet = TheParser->findContextSensitiveInfixParselet(Tok.Tok);
                
                auto PatSeq = std::unique_ptr<NodeSeq>(new NodeSeq);
                PatSeq->reserve(1 + 1);
                PatSeq->append(std::move(Pat));
                PatSeq->append(std::move(ArgsTest));
                return colonParselet->parseContextSensitive(std::move(PatSeq), Ctxt);
            }
        }
        
        //
        // Prepend in correct order
        //
        
        TheParser->prependInReverse(std::move(ArgsTest));
        
        return Pat;
    }
}

//
// Something like  a ~f~ b
//
NodePtr TildeParselet::parse(std::unique_ptr<NodeSeq> Left, ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1 + 1 + 1 + 1);
    
    Args->append(std::move(Left));
    
    auto FirstTilde = TheParser->currentToken();
    
    Args->append(std::unique_ptr<Node>(new LeafNode(FirstTilde)));
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = getPrecedence();
    Ctxt.Assoc = ASSOCIATIVITY_NONE;
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    Tok = Parser::eatAll(Tok, Ctxt, Args);
    
    Utils::differentLineWarning(FirstTilde, Tok, SYNTAXISSUESEVERITY_FORMATTING);
    
    auto FirstTok = Tok;
    
    auto Middle = TheParser->parse(Ctxt);
    
    Args->append(std::move(Middle));
    
    Tok = TheParser->currentToken();
    
    Tok = Parser::eatAll(Tok, Ctxt, Args);
    
    Utils::differentLineWarning(FirstTok, Tok, SYNTAXISSUESEVERITY_FORMATTING);
    
    if (Tok.Tok == TOKEN_TILDE) {
        
        Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
        
        Tok = TheParser->nextToken(Ctxt);
        
        Tok = Parser::eatAll(Tok, Ctxt, Args);
        
        auto Right = TheParser->parse(Ctxt);
        
        Args->append(std::move(Right));
        
        return std::unique_ptr<Node>(new TernaryNode(SYMBOL_AST_TERNARYTILDE, std::move(Args)));
    }
    
    //
    // Something like   a ~f b
    //
    // Proceed with parsing as if the second ~ were present
    //
    
    if (Tok.Tok == TOKEN_ENDOFFILE) {
        
        //
        // If EndOfFile, don't bother trying to parse
        //
        
        Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
        
        auto Error = std::unique_ptr<Node>(new SyntaxErrorNode(SYNTAXERROR_EXPECTEDTILDE, std::move(Args)));
        
        return Error;
    }
    
    auto Right = TheParser->parse(Ctxt);
    
    Args->append(std::move(Right));
    
    auto Error = std::unique_ptr<Node>(new SyntaxErrorNode(SYNTAXERROR_EXPECTEDTILDE, std::move(Args)));
    
    return Error;
}



//
// Something like  symbol:object
//
// when parsing a in a:b  then ColonFlag is false
// when parsing b in a:b  then ColonFlag is true
//
NodePtr ColonParselet::parse(std::unique_ptr<NodeSeq> Left, ParserContext CtxtIn) const {
    
    assert((CtxtIn.Flag & PARSER_COLON) != PARSER_COLON);
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1 + 1);
    
    Args->append(std::move(Left));
    
    auto TokIn = TheParser->currentToken();
    
    Args->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = PRECEDENCE_FAKE_PATTERNCOLON;
    Ctxt.Assoc = ASSOCIATIVITY_NONE;
    Ctxt.Flag |= PARSER_COLON;
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    Tok = Parser::eatAll(Tok, Ctxt, Args);
    
    auto Right = TheParser->parse(Ctxt);
    
    Args->append(std::move(Right));

    if ((CtxtIn.Flag & PARSER_PARSED_SYMBOL) != PARSER_PARSED_SYMBOL) {

        auto Error = std::unique_ptr<Node>(new SyntaxErrorNode(SYNTAXERROR_COLONERROR, std::move(Args)));

        return Error;
    }
    
    Ctxt.Flag.clear(PARSER_PARSED_SYMBOL);
    
    auto Pat = std::unique_ptr<Node>(new BinaryNode(SYMBOL_PATTERN, std::move(Args)));

    //
    // LOOKAHEAD
    //
    {
        auto ArgsTest = std::unique_ptr<LeafSeq>(new LeafSeq);
        
        Tok = TheParser->currentToken();
        
        Tok = Parser::eatAndPreserveToplevelNewlines(Tok, CtxtIn, ArgsTest);
        
        if (Tok.Tok == TOKEN_COLON) {
            
            Ctxt.Flag.clear(PARSER_COLON);
            
            auto PatSeq = std::unique_ptr<NodeSeq>(new NodeSeq);
            
            PatSeq->append(std::move(Pat));
            PatSeq->append(std::move(ArgsTest));
            
            return parseContextSensitive(std::move(PatSeq), Ctxt);
        }
        
        //
        // Prepend in correct order
        //
        
        TheParser->prependInReverse(std::move(ArgsTest));
        
        return Pat;
    }
}

//
// Something like  pattern:optional
//
// Called from other parselets
//
NodePtr ColonParselet::parseContextSensitive(std::unique_ptr<NodeSeq> Left, ParserContext CtxtIn) const {
    
    //
    // when parsing a in a:b  then ColonFlag is false
    // when parsing b in a:b  then ColonFlag is true
    //
    assert((CtxtIn.Flag & PARSER_COLON) != PARSER_COLON);
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1 + 1);
    
    Args->append(std::move(Left));
    
    auto TokIn = TheParser->currentToken();
    
    Args->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = PRECEDENCE_FAKE_OPTIONALCOLON;
    Ctxt.Assoc = ASSOCIATIVITY_NONE;
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    Tok = Parser::eatAll(Tok, Ctxt, Args);
    
    auto Right = TheParser->parse(Ctxt);
    
    Args->append(std::move(Right));
    
    return std::unique_ptr<Node>(new BinaryNode(SYMBOL_OPTIONAL, std::move(Args)));
}

//
// Something like  a /: b = c
//
// a   /:   b   =   c
// ^~~~~ Args at the start
//       ^~~ ArgsTest1
//           ^~~ ArgsTest2
//
NodePtr SlashColonParselet::parse(std::unique_ptr<NodeSeq> Left, ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1 + 1);
    
    Args->append(std::move(Left));
    
    auto TokIn = TheParser->currentToken();
    
    Args->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = getPrecedence();
    Ctxt.Assoc = getAssociativity();
    
    
    auto ArgsTest1 = std::unique_ptr<LeafSeq>(new LeafSeq);
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    Parser::eatAll(Tok, Ctxt, ArgsTest1);
    
    auto Middle = TheParser->parse(Ctxt);
    
    
    auto ArgsTest2 = std::unique_ptr<LeafSeq>(new LeafSeq);
    
    Tok = TheParser->currentToken();
    
    Tok = Parser::eatAll(Tok, Ctxt, ArgsTest2);
    
    if (Tok.Tok == TOKEN_EQUAL) {
        
        Args->append(std::move(ArgsTest1));
        Args->append(std::move(Middle));
        Args->append(std::move(ArgsTest2));
        
        Ctxt.Flag |= PARSER_INSIDE_SLASHCOLON;
        
        auto& equalParselet = TheParser->findInfixParselet(Tok.Tok);
        
        auto N = equalParselet->parse(std::move(Args), Ctxt);
        
        return N;
        
    } else if (Tok.Tok == TOKEN_COLONEQUAL) {
        
        auto Args2 = std::unique_ptr<NodeSeq>(new NodeSeq);
        Args2->append(std::move(Middle));
        Args2->append(std::move(ArgsTest2));
        
        auto& colonEqualParselet = TheParser->findInfixParselet(Tok.Tok);
        
        auto N = colonEqualParselet->parse(std::move(Args2), Ctxt);
        
        Args->append(std::move(ArgsTest1));
        
        auto C = N->getChildrenDestructive();
        
        Args->append(std::unique_ptr<NodeSeq>(C));
        
        return std::unique_ptr<Node>(new TernaryNode(SYMBOL_TAGSETDELAYED, std::move(Args)));
        
    } else if (Tok.Tok == TOKEN_EQUALDOT) {
        
        auto Args2 = std::unique_ptr<NodeSeq>(new NodeSeq);
        Args2->append(std::move(Middle));
        Args2->append(std::move(ArgsTest2));
        
        auto& equalParselet = TheParser->findInfixParselet(Tok.Tok);
        
        auto N = equalParselet->parse(std::move(Args2), Ctxt);
        
        Args->append(std::move(ArgsTest1));
        
        auto C = N->getChildrenDestructive();
        
        Args->append(std::unique_ptr<NodeSeq>(C));
        
        return std::unique_ptr<Node>(new TernaryNode(SYMBOL_TAGUNSET, std::move(Args)));
    }
    
    //
    // Anything other than:
    // a /: b = c
    // a /: b := c
    // a /: b =.
    //
    
    Args->append(std::move(ArgsTest1));
    Args->append(std::move(Middle));
    Args->append(std::move(ArgsTest2));
    //
    // Important to not duplicate token's Str here, it may also appear later
    //
    // Also, invent Source
    //
    
    auto createdToken = Token(TOKEN_ERROR_EXPECTEDOPERAND, "", Source(Tok.Src.start()));
    
    Args->append(std::unique_ptr<Node>(new LeafNode(createdToken)));
    
    auto Error = std::unique_ptr<Node>(new SyntaxErrorNode(SYNTAXERROR_EXPECTEDSET, std::move(Args)));
    
    return Error;
}



//
// Something like  \( x \)
//
NodePtr LinearSyntaxOpenParenParselet::parse(ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1 + 1);
    
    auto Opener = TheParser->currentToken();
    
    auto Ctxt = CtxtIn;
    Ctxt.GroupDepth++;
    Ctxt.Flag |= PARSER_LINEARSYNTAX;
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    auto CloserTok = TOKEN_LINEARSYNTAX_CLOSEPAREN;
    Ctxt.Closer = CloserTok;
    
    Args->append(std::unique_ptr<Node>(new LeafNode(Opener)));
    
    while (true) {
        
        //
        // Check isAbort() inside loops
        //
        if (TheParser->isAbort()) {
            
            auto A = Token(TOKEN_ERROR_ABORTED, "", Source(TheSourceManager->getSourceLocation()));
            
            auto Aborted = std::unique_ptr<Node>(new LeafNode(A));
            
            return Aborted;
        }
        
        
        if (Tok.Tok == TOKEN_ENDOFFILE) {
            
            //
            // Handle something like   \( a EOF
            //
            
            auto group = std::unique_ptr<Node>(new GroupMissingCloserNode(SYMBOL_AST_GROUPLINEARSYNTAXPAREN, std::move(Args)));
            
            return group;
        }
        if (Tok.Tok == CloserTok) {
            
            Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
            
            auto Ctxt2 = Ctxt;
            Ctxt2.GroupDepth--;
            
            TheParser->nextToken(Ctxt2);
            
            break;
        }
        
        //
        // Do not check for other closers here
        //
        // As long as \( \) parses by just doing tokenization, then cannot reliably test for other closers
        //
        // e.g., \( ( \) is completely valid syntax
        //
        
        if (Tok.Tok == TOKEN_LINEARSYNTAX_OPENPAREN) {
            
            auto Sub = this->parse(Ctxt);

#ifndef NDEBUG
            auto S = Sub.get();
            
            if (auto SubOpenParen = dynamic_cast<GroupNode*>(S)) {
                
                assert(SubOpenParen->getOperator() == SYMBOL_AST_GROUPLINEARSYNTAXPAREN);
                
            } else if (auto SubOpenParen = dynamic_cast<GroupMissingCloserNode*>(S)) {
                
                assert(SubOpenParen->getOperator() == SYMBOL_AST_GROUPLINEARSYNTAXPAREN);
                
            } else {
                assert(false);
            }
#endif
            Args->append(std::move(Sub));
            
            Tok = TheParser->currentToken();
            
        } else {
            
            //
            // COMMENT, WHITESPACE, and NEWLINE are handled here
            //
            
            Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
            
            Tok = TheParser->nextToken(Ctxt);
        }
        
    } // while
    
    return std::unique_ptr<Node>(new GroupNode(SYMBOL_AST_GROUPLINEARSYNTAXPAREN, std::move(Args)));
}

//
// Something like  a =.
//
// a /: b = c  and  a /: b = .  are also handled here
//
NodePtr EqualParselet::parse(std::unique_ptr<NodeSeq> Left, ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1 + 1);
    
    Args->append(std::move(Left));
    
    auto TokIn = TheParser->currentToken();
    
    Args->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = getPrecedence();
    Ctxt.Assoc = getAssociativity();
    
    if (TokIn.Tok == TOKEN_EQUALDOT) {
        
        TheParser->nextToken(Ctxt);
        
        if ((Ctxt.Flag & PARSER_INSIDE_SLASHCOLON) == PARSER_INSIDE_SLASHCOLON) {
            return std::unique_ptr<Node>(new TernaryNode(SYMBOL_TAGUNSET, std::move(Args)));
        }
        
        return std::unique_ptr<Node>(new BinaryNode(SYMBOL_UNSET, std::move(Args)));
    }
    
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    Tok = Parser::eatAll(Tok, Ctxt, Args);
    
    if (Tok.Tok == TOKEN_DOT) {
        
        //
        // Something like a = .
        //
        // tutorial/OperatorInputForms
        // Spaces to Avoid
        //
        
        Utils::notContiguousWarning(TokIn, Tok);
        
        TheParser->nextToken(Ctxt);
        
        Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
        
        if ((Ctxt.Flag & PARSER_INSIDE_SLASHCOLON) == PARSER_INSIDE_SLASHCOLON) {
            return std::unique_ptr<Node>(new TernaryNode(SYMBOL_TAGUNSET, std::move(Args)));
        }
        
        return std::unique_ptr<Node>(new BinaryNode(SYMBOL_UNSET, std::move(Args)));
    }
    
    auto Right = TheParser->parse(Ctxt);
    
    Args->append(std::move(Right));
    
    if ((Ctxt.Flag & PARSER_INSIDE_SLASHCOLON) == PARSER_INSIDE_SLASHCOLON) {
        return std::unique_ptr<Node>(new TernaryNode(SYMBOL_TAGSET, std::move(Args)));
    }
    
    return std::unique_ptr<Node>(new BinaryNode(SYMBOL_SET, std::move(Args)));
}


//
// Something like  \[Integral] f \[DifferentialD] x
//
NodePtr IntegralParselet::parse(ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1 + 1);
    
    auto TokIn = TheParser->currentToken();
    
    Args->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = getPrecedence();
    Ctxt.Assoc = ASSOCIATIVITY_NONE;
    Ctxt.Flag |= PARSER_INTEGRAL;
    
    auto Tok = TheParser->nextToken(Ctxt);
    
    Tok = Parser::eatAll(Tok, Ctxt, Args);
    
    Utils::differentLineWarning(TokIn, Tok, SYNTAXISSUESEVERITY_FORMATTING);
    
    auto operand = TheParser->parse(Ctxt);
    
    Args->append(std::move(operand));
    
    Ctxt.Flag.clear(PARSER_INTEGRAL);
    
    
    //
    // LOOKAHEAD
    //
    {
        auto ArgsTest = std::unique_ptr<LeafSeq>(new LeafSeq);
        
        Tok = TheParser->currentToken();
        
        Tok = Parser::eatAll(Tok, Ctxt, ArgsTest);
        
        if (Tok.Tok != TOKEN_LONGNAME_DIFFERENTIALD) {
            
            //
            // Prepend in correct order
            //
            TheParser->prependInReverse(std::move(ArgsTest));
            
            return std::unique_ptr<Node>(new PrefixNode(PrefixOperatorToSymbol(TokIn.Tok), std::move(Args)));
        }
            
        Utils::differentLineWarning(TokIn, Tok, SYNTAXISSUESEVERITY_FORMATTING);
        
        auto& differentialDparselet = TheParser->findPrefixParselet(Tok.Tok);
        
        auto variable = differentialDparselet->parse(Ctxt);
        
        Args->append(std::move(ArgsTest));
        Args->append(std::move(variable));
        
        return std::unique_ptr<Node>(new PrefixBinaryNode(PrefixBinaryOperatorToSymbol(TokIn.Tok), std::move(Args)));
    }
}

//
// Gather all < > == <= => into a single node
//
NodePtr InequalityParselet::parse(std::unique_ptr<NodeSeq> Left, ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    
    Args->append(std::move(Left));
    
    auto TokIn = TheParser->currentToken();
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = getPrecedence();
    Ctxt.Assoc = ASSOCIATIVITY_NONE;
    
    while (true) {
        
        //
        // Check isAbort() inside loops
        //
        if (TheParser->isAbort()) {
            
            auto A = Token(TOKEN_ERROR_ABORTED, "", Source(TheSourceManager->getSourceLocation()));
            
            auto Aborted = std::unique_ptr<Node>(new LeafNode(A));
            
            return Aborted;
        }
        
        
        //
        // LOOKAHEAD
        //
        {
            auto ArgsTest = std::unique_ptr<LeafSeq>(new LeafSeq);
            
            auto Tok = TheParser->currentToken();
            
            Tok = Parser::eatAndPreserveToplevelNewlines(Tok, CtxtIn, ArgsTest);
            
            if (isInequalityOperator(Tok.Tok)) {
                
                Args->append(std::move(ArgsTest));
                Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
                
                Tok = TheParser->nextToken(Ctxt);
                
                Tok = Parser::eatAll(Tok, Ctxt, Args);
                
                auto operand = TheParser->parse(Ctxt);
                
                Args->append(std::move(operand));
                
            } else {
                
                //
                // Tok.Tok != TokIn.Tok, so break
                //
                //
                // Prepend in correct order
                //
                TheParser->prependInReverse(std::move(ArgsTest));
                
                break;
            }
        }
        
    } // while
    
    return std::unique_ptr<Node>(new InfixNode(SYMBOL_INEQUALITY, std::move(Args)));
}

//
// Gather all \[VectorGreater] \[VectorLess] \[VectorGreaterEqual] \[VectorLessEqual] into a single node
//
NodePtr VectorInequalityParselet::parse(std::unique_ptr<NodeSeq> Left, ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    
    Args->append(std::move(Left));
    
    auto TokIn = TheParser->currentToken();
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = getPrecedence();
    Ctxt.Assoc = ASSOCIATIVITY_NONE;
    
    while (true) {
        
        //
        // Check isAbort() inside loops
        //
        if (TheParser->isAbort()) {
            
            auto A = Token(TOKEN_ERROR_ABORTED, "", Source(TheSourceManager->getSourceLocation()));
            
            auto Aborted = std::unique_ptr<Node>(new LeafNode(A));
            
            return Aborted;
        }
        
        //
        // LOOKAHEAD
        //
        {
            auto ArgsTest = std::unique_ptr<LeafSeq>(new LeafSeq);
            
            auto Tok = TheParser->currentToken();
            
            Tok = Parser::eatAndPreserveToplevelNewlines(Tok, CtxtIn, ArgsTest);
            
            if (isVectorInequalityOperator(Tok.Tok)) {
                
                Args->append(std::move(ArgsTest));
                Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
                
                Tok = TheParser->nextToken(Ctxt);
                
                Tok = Parser::eatAll(Tok, Ctxt, Args);
                
                auto operand = TheParser->parse(Ctxt);
                
                Args->append(std::move(operand));
                
            } else {
                
                //
                // Tok.Tok != TokIn.Tok, so break
                //
                //
                // Prepend in correct order
                //
                TheParser->prependInReverse(std::move(ArgsTest));
                
                return std::unique_ptr<Node>(new InfixNode(SYMBOL_DEVELOPER_VECTORINEQUALITY, std::move(Args)));
            }
        }
        
    } // while
}

NodePtr InfixOperatorWithTrailingParselet::parse(std::unique_ptr<NodeSeq> Left, ParserContext CtxtIn) const {
    
    auto Args = std::unique_ptr<NodeSeq>(new NodeSeq);
    Args->reserve(1 + 1 + 1);
    
    Args->append(std::move(Left));
    
    auto TokIn = TheParser->currentToken();
    
    auto lastOperatorToken = TokIn;
    
    auto Ctxt = CtxtIn;
    Ctxt.Prec = getPrecedence();
    Ctxt.Assoc = ASSOCIATIVITY_NONE;
    
    while (true) {
        
        //
        // Check isAbort() inside loops
        //
        if (TheParser->isAbort()) {
            
            auto A = Token(TOKEN_ERROR_ABORTED, "", Source(TheSourceManager->getSourceLocation()));
            
            auto Aborted = std::unique_ptr<Node>(new LeafNode(A));
            
            return Aborted;
        }
        
        
        //
        // LOOKAHEAD
        //
        {
            auto ArgsTest1 = std::unique_ptr<LeafSeq>(new LeafSeq);
            
            auto Tok = TheParser->currentToken();
            
            Tok = Parser::eatAndPreserveToplevelNewlines(Tok, CtxtIn, ArgsTest1);
            
            //
            // Cannot just compare tokens
            //
            // May be something like  a * b c \[Times] d
            //
            // and we want only a single Infix node created
            //
            if (isInfixOperator(Tok.Tok) &&
                InfixOperatorToSymbol(Tok.Tok) == InfixOperatorToSymbol(TokIn.Tok)) {
                
                Args->append(std::move(ArgsTest1));
                Args->append(std::unique_ptr<Node>(new LeafNode(Tok)));
                
                lastOperatorToken = Tok;
                
                //
                // ALLOWTRAILING CODE
                //
                
                //
                // Something like  a;b  or  a,b
                //
                
                Tok = TheParser->nextToken(Ctxt);
                
                //
                // LOOKAHEAD
                //
                {
                    auto ArgsTest2 = std::unique_ptr<LeafSeq>(new LeafSeq);
                    
                    Tok = Parser::eatAndPreserveToplevelNewlines(Tok, Ctxt, ArgsTest2);
                    
                    if (isInfixOperator(Tok.Tok) &&
                        InfixOperatorToSymbol(Tok.Tok) == InfixOperatorToSymbol(TokIn.Tok)) {
                        
                        //
                        // Something like  a; ;
                        //
                        
                        auto Implicit = Token(TOKEN_FAKE_IMPLICITNULL, "", Source(lastOperatorToken.Src.start()));
                        
                        lastOperatorToken = Tok;
                        
                        Args->append(std::unique_ptr<Node>(new LeafNode(Implicit)));
                        
                        Args->append(std::move(ArgsTest2));
                        
                    } else if (TheParser->isPossibleBeginningOfExpression(Tok, Ctxt)) {
                        
                        auto operand = TheParser->parse(Ctxt);
                        
                        Args->append(std::move(ArgsTest2));
                        
                        Args->append(std::move(operand));
                        
                    } else {
                        
                        //
                        // Not beginning of an expression
                        //
                        // For example:  a;&
                        //
                        
                        auto Implicit = Token(TOKEN_FAKE_IMPLICITNULL, "", Source(lastOperatorToken.Src.end()));
                        
                        Args->append(std::unique_ptr<Node>(new LeafNode(Implicit)));
                        
                        //
                        // Prepend in correct order
                        //
                        TheParser->prependInReverse(std::move(ArgsTest2));
                        
                        return std::unique_ptr<Node>(new InfixNode(InfixOperatorToSymbol(TokIn.Tok), std::move(Args)));
                    }
                }
                
            } else {
                
                //
                // Prepend in correct order
                //
                TheParser->prependInReverse(std::move(ArgsTest1));
                
                return std::unique_ptr<Node>(new InfixNode(InfixOperatorToSymbol(TokIn.Tok), std::move(Args)));
            }
        }
        
    } // while
}

//
// Error handling and Cleanup
//

NodePtr ExpectedPossibleExpressionErrorParselet::parse(ParserContext CtxtIn) const {
    
    auto TokIn = TheParser->currentToken();
    
    auto Ctxt = CtxtIn;
    
    TheParser->nextToken(Ctxt);
    
    if (isError(TokIn.Tok)) {
        
        //
        // If there is a Token error, then use that specific error
        //
        
        auto SyntaxErrorEnum = TokenErrorToSyntaxError(TokIn.Tok);
        
        auto TmpVec = std::unique_ptr<NodeSeq>(new NodeSeq);
        
        TmpVec->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
        
        auto Error = std::unique_ptr<Node>(new SyntaxErrorNode(SyntaxErrorEnum, std::move(TmpVec)));
        
        return Error;
    }
    
    //
    // If NOT a Token error, then just use a generic error
    //
    
    auto SyntaxErrorEnum = SYNTAXERROR_EXPECTEDPOSSIBLEEXPRESSION;
    
    auto TmpVec = std::unique_ptr<NodeSeq>(new NodeSeq);
    
    TmpVec->append(std::unique_ptr<Node>(new LeafNode(TokIn)));
    
    auto Error = std::unique_ptr<Node>(new SyntaxErrorNode(SyntaxErrorEnum, std::move(TmpVec)));
    
    return Error;
}




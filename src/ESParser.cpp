#include "Pch.hpp"
#include "ESParser.hpp"
#include "SeddEcException.hpp"

#define TOKEN_TYPE_TABLE \
X(LPAREN) \
X(RPAREN) \
X(OPERATOR) \
X(IDENTIFIER) \
X(STRING) \
X(INTEGER)

#define X(a) a,
enum Type {
  TOKEN_TYPE_TABLE
};
#undef X

#define X(a) #a,
string tokenName[] = {
  TOKEN_TYPE_TABLE
};
#undef X

struct Token {
    Type type;
    string value;
};

static unsigned lineNum = 0;
static unsigned colNum;

#define AT_FORMAT " at line " << lineNum << " column " << colNum

Token NextToken(std::stringstream& input) {
    std::stringstream value;
    int c;

GET_FIRST:
    {
        ++colNum; c = input.get();
        switch(c) {
        case EOF:
            throw SeddEcException(Reason::INVALID_INPUT_FORMAT, FORMAT("Unexpected end-of-line" << AT_FORMAT));
        case ' ':
            goto GET_FIRST;
        case '(':
        case '{':
        case '[':
            value << (char)c;
            return Token{ LPAREN, value.str() };
        case ')':
        case '}':
        case ']':
            value << (char)c;
            return Token{ RPAREN, value.str() };
        case '=':
        case ',':
            value << (char)c;
            return Token{ OPERATOR, value.str() };
        case '\"':
            goto SCAN_STRING;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            value << (char)c;
            goto SCAN_INTEGER;
        default:
            value << (char)c;
            goto SCAN_ID;
        }
    }

SCAN_ID:
    {
        c = input.peek();
        switch(c) {
        case EOF:
        case ' ':
        case '(':
        case '{':
        case '[':
        case ')':
        case '}':
        case ']':
        case '=':
        case ',':
        case '\"':
            return Token{ IDENTIFIER, value.str() };
        default:
            ++colNum; input.get();
            value << (char)c;
            goto SCAN_ID;
        }
    }

SCAN_STRING:
    {
        c = input.peek();
        switch(c) {
        case EOF:
            throw SeddEcException(Reason::INVALID_INPUT_FORMAT, FORMAT("Unmatched \"" << AT_FORMAT));
        case '\"':
            ++colNum; input.get();
            return Token{ STRING, value.str() };
        default:
            ++colNum; input.get();
            value << (char)c;
            goto SCAN_STRING;
        }
    }

SCAN_INTEGER:
    {
        c = input.peek();
        switch(c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ++colNum; input.get();
            value << (char)c;
            goto SCAN_INTEGER;
        default:
            return Token{ INTEGER, value.str() };
        }
    }
}

void Expect(Token token, Type type) {
    if (token.type != type)
        throw SeddEcException(Reason::INVALID_INPUT_FORMAT, FORMAT("Unexpected token: " << token.value << " (" << tokenName[token.type] << ")" << AT_FORMAT
            << " Expected (" << tokenName[type] << ")"));
}

void Expect(Token token, Type type, string value) {
    if (token.type != type || token.value != value)
        throw SeddEcException(Reason::INVALID_INPUT_FORMAT, FORMAT("Unexpected token: " << token.value << AT_FORMAT
            << " Expected " << value << " (" << tokenName[type] << ")"));
}

void IgnoreLevel(std::stringstream& input) {
    Token token;
    do {
        token = NextToken(input);
        if (token.type == LPAREN)
            IgnoreLevel(input);
    } while(token.type != RPAREN);
}

void IgnoreExpression(std::stringstream& input) {
    auto token = NextToken(input);
    if (token.type == LPAREN)
        IgnoreLevel(input);
}

vector<unsigned> ParseIdList(std::stringstream& input) {
    vector<unsigned> idList;

    Expect(NextToken(input), LPAREN);
    bool first = true;
    for (;;) {
        auto token = NextToken(input);
        if (token.type == RPAREN)
            return idList;
        if (!first) {
            Expect(token, OPERATOR, ",");
            token = NextToken(input);
        }
        Expect(token, INTEGER);
        idList.emplace_back(std::stoi(token.value));
        first = false;
    }
}

Event ParseEvent(std::stringstream& input) {
    Event event;

    Expect(NextToken(input), LPAREN);
    auto idToken = NextToken(input);
    Expect(idToken, INTEGER);
    event.id = std::stoi(idToken.value);

    Expect(NextToken(input), OPERATOR, ",");
    Expect(NextToken(input), IDENTIFIER, "Event");
    
    Expect(NextToken(input), LPAREN);
    bool first = true;
    for (;;) {
        auto token = NextToken(input);
        if (token.type == RPAREN)
            break;
        if (!first) {
            Expect(token, OPERATOR, ",");
            token = NextToken(input);
        }
        Expect(token, IDENTIFIER);
        if (token.value == "pred") {
            Expect(NextToken(input), OPERATOR, "=");
            event.predecessors = ParseIdList(input);
        }
        else if (token.value == "icnf") {
            Expect(NextToken(input), OPERATOR, "=");
            event.conflicts = ParseIdList(input);
        } else {
            Expect(NextToken(input), OPERATOR, "=");
            IgnoreExpression(input);
        }
        first = false;
    }

    Expect(NextToken(input), RPAREN);

    return event;
}

vector<Event> ParseEventStructure(string path) {
    std::ifstream file(path);

    vector<Event> events;

    string line;
    for(;std::getline(file, line);) {
        ++lineNum;
        std::stringstream remainingLine{ line };
        events.emplace_back(ParseEvent(remainingLine));
    }

    return events;
}

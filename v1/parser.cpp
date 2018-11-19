#include <iostream>
#include <stdlib.h>
#include <vector>
#include "ast.h"
#include "tokeniser.h"
#include "j-tok.h"
#include "symbols.h"

using namespace std;

static tokeniser tokens;
static Token token;

class variable: public symbol_value
{
public:
	jn_var var;
	variable(jn_var v): var(v){}
};

class funcscope
{
public:
	string name;				//function name
	string type;				//function type: constructor method function
	symbol_table_values symbols;//function scope symbols
	int arguments, locals;      //function scope variable counts
	funcscope(string n, string t):name(n), type(t), arguments(0), locals(0)
	{
		symbols = create_symbol_table_values();
		if(type.compare("method") == 0)
			arguments = 1;
	}
	jn_var lookup(string n)
	{
		variable* v = (variable *)symbols->lookup(n);
		return v == nullptr? nullptr: v->var;
	}
	void insert(jn_var v)
	{
		symbols->insert(v->name, new variable(v));
	}
	int getIndex(string type)
	{
		if(type.compare("local") == 0)
			return locals++;
		else
			return arguments++;
	}
};

class classscope
{
public:
	string name;   				//classname
	vector<funcscope*>funcs;
	symbol_table_values symbols;
	int statics, fields;
	funcscope *curf;
	classscope(string n): name(n), statics(0), fields(0)
	{
		symbols = create_symbol_table_values();
		curf = nullptr;
	}
	void addfunc(string n, string t)	
	{
		funcscope *f = new funcscope(n, t);
		funcs.push_back(f);
		curf = f;
	}
	funcscope* findfunc(string n)
	{
		for(int i = 0; i < funcs.size(); i++)
			if(funcs[i]->name == n)
				return funcs[i];
	}
	int getIndex(string type)
	{
		if(type.compare("static") == 0)
			return this->statics++;
		else if(type.compare("this") == 0)
			return this->fields++;
		return curf->getIndex(type);
	}
};

vector<classscope *>classes;	//The symbol table
classscope *currentclass = nullptr;	//The current class

void newClass(string name)
{
	currentclass = new classscope(name);
	classes.push_back(currentclass);
}

void newFunc(string name, string type)
{
	currentclass->addfunc(name, type);
}

void insert(jn_var var)
{
	if(currentclass->curf == nullptr)
		currentclass->symbols->insert(var->name, new variable(var));
	else
		currentclass->curf->insert(var);
}

jn_var lookup(string name)
{
	variable *v = (variable *)currentclass->symbols->lookup(name);
	if(v != nullptr)
		return v->var;
	if(currentclass->curf != nullptr)
		return currentclass->curf->lookup(name);
	return nullptr;
}

bool methodType(string c, string n)
{
	for(classscope* cl : classes)
		if(cl->name == c)
		{
			for(funcscope* f: cl->funcs)
				if(f->name == n)
					return f->type == "method";
		}
	return true;
}

void nextToken()
{
	token = tokens->next_token();
}

void mustbe(TokenKind expected)
{
	if(expected != token.token)
		exit(-1); 
	nextToken();
}

void mustbe(vector<TokenKind>v)
{
	for(TokenKind t: v)
		if(t == token.token)
		{
			nextToken();
			return;
		}
	exit(0);
}

bool have(TokenKind expected)
{
	if(expected != token.token)
		return false;
	nextToken();
	return true;
}

bool in(vector<TokenKind>v)
{
	for(TokenKind k: v)
		if(token.token == k)
			return true;
	return false;
}

//Not read next token.
bool is(TokenKind expected)
{
	return expected == token.token;
}

//parsing function declarations.
jn_class parseClass();
jn_var_decs parseVarDeclarations();
jn_subr_decs parseSubroutines();
jn_statements parseStatements();
jn_expr parseExpresion();

jn_class parseClass()
{
	mustbe(jk_class);
	newClass(token.spelling);
	nextToken();
	mustbe(jk_lcb);
	jn_var_decs decs = parseVarDeclarations();
	jn_subr_decs subrs = parseSubroutines();
	return jn_class_create(currentclass->name, decs, subrs);
}

jn_var_decs parseVarDeclarations()
{
	jn_var_decs decs = jn_var_decs_create();
	int offset;
	while(is(jk_static) || is(jk_field) || is(jk_var))
	{
		string segment = token.spelling;
		if(segment == "field")
			segment = "this";
		else if(segment == "var")
			segment = "local";
		nextToken();
		string type = token.spelling;
		mustbe({jk_int, jk_char, jk_boolean, jk_identifier});
		do{
			string id = token.spelling;
			mustbe(jk_identifier);
			offset = currentclass->getIndex(segment);
			jn_var_dec dec = jn_var_dec_create(segment, id, offset, type);
			jn_var var = jn_var_create(segment, id, dec->offset, type);
			insert(var);
			decs->append(dec);
		}while(have(jk_comma));
		mustbe(jk_semi);
	}
	return decs;
}

jn_param_list parseParamList()
{
	jn_param_list pl = jn_param_list_create();
	while(true)
	{
		if(is(jk_rrb))
			break;
		string type = token.spelling;
		mustbe({jk_int, jk_boolean, jk_char, jk_identifier});
		string name = token.spelling;		
		int offset = currentclass->getIndex("argument");
		jn_var_dec dec = jn_var_dec_create("argument", name, offset, type);
		pl->append(dec);
		jn_var var = jn_var_create("argument", name, dec->offset, type);
		insert(var);
		nextToken();
		if(is(jk_comma))
			mustbe(jk_comma);
	}	
	return pl;
}

jn_return parseReturnStmt()
{
	jn_return ret = nullptr;
	mustbe(jk_return);
	if(!is(jk_semi))
	{
		jn_expr expr = parseExpresion();
		ret = to_jn_return(jn_return_expr_create(expr));
	}
	else
		ret = jn_return_create();
	mustbe(jk_semi);
	return ret;
}

jn_expr_list parseExprList()
{
	jn_expr_list list = jn_expr_list_create();
	while(true)
	{
		if(is(jk_rrb))
			break;
		jn_expr e = parseExpresion();
		list->append(e);
		if(is(jk_comma))
			nextToken();
	}
	return list;
}

jn_expr parseTerm()
{
	//The following four 'true', 'false', 'null', 'this' are keyword constants
	if(is(jk_true))
	{
		nextToken();
		return to_jn_expr(jn_bool_create(true));
	}
	else if(is(jk_false))
	{
		nextToken();
		return to_jn_expr(jn_bool_create(false));
	}
	else if(is(jk_null))
	{
		nextToken();
		return to_jn_expr(jn_null_create());
	}
	else if(is(jk_this))
	{
		nextToken();
		return to_jn_expr(jn_this_create());
	}
	else if(is(jk_integerConstant)) //integer constant
	{
		jn_int value = jn_int_create(atoi(token.spelling.c_str()));
		nextToken();
		return to_jn_expr(value);
	}
	else if(is(jk_stringConstant))
	{
		jn_string str = jn_string_create(token.spelling);
		nextToken();
		return to_jn_expr(str);
	}
	else if(is(jk_lrb)) //(expresion)
	{
		nextToken();
		jn_expr expr= parseExpresion();
		mustbe(jk_rrb);
		return to_jn_expr(expr);
	}
	else if(is(jk_sub) || is(jk_not))
	{
		string op = token.spelling;
		nextToken();
		jn_expr expr = parseExpresion();
		jn_unary_op unary = jn_unary_op_create(op, expr);
		return to_jn_expr(unary);
	}
	else if(is(jk_identifier))	//varName, arrayIndex or subroutineCall
	{
		string s = token.spelling;	
		jn_var v = lookup(token.spelling);
		nextToken();
		if(is(jk_stop))  //a.function()
		{
			nextToken();
			string className = (v == nullptr) ? s: v->type;
			string subr_name = token.spelling;
			bool type = (v != nullptr);
			nextToken();
			mustbe(jk_lrb);
			jn_expr_list params = parseExprList();
			if(v != nullptr)
			{
				jn_var var = jn_var_create(v->segment, v->name, v->offset, v->type);
				params->prefix(to_jn_expr(var));
			}
			mustbe(jk_rrb);
			return to_jn_expr(jn_call_create(type, className, subr_name, params));
		}
		else if(is(jk_lrb)) //function()
		{
			string className = currentclass->name;
			string subr_name = s;
			bool type = methodType(className, subr_name);
			mustbe(jk_lrb);
			jn_expr_list params = parseExprList();
			params->prefix(to_jn_expr(jn_this_create()));
			mustbe(jk_rrb);
			return to_jn_expr(jn_call_create(type, className, subr_name, params));
		}	
		else if(is(jk_lsb)) //a[]
		{
			jn_var var = jn_var_create(v->segment, v->name, v->offset, v->type);
			mustbe(jk_lsb);
			jn_expr expr = parseExpresion();
			mustbe(jk_rsb);
			return to_jn_expr(jn_array_index_create(var, expr));	
		}
		else
		{	
			jn_var var = jn_var_create(v->segment, v->name, v->offset, v->type);
			return to_jn_expr(var);
		}
	}
	return nullptr;
}

jn_expr parseExpresion()
{
	jn_expr term = parseTerm();
	if(in({jk_add, jk_sub, jk_times, jk_divide, jk_and, jk_or, jk_lt, jk_gt, jk_eq}))
	{
		string op = token.spelling;
		nextToken();
		jn_expr term1 = parseExpresion();
		jn_infix_op infix = jn_infix_op_create(term, op, term1);
		return to_jn_expr(infix);
	}
	return term;
}

jn_while parseWhileStmt()
{
	mustbe(jk_while);
	mustbe(jk_lrb);
	jn_expr cond = parseExpresion();
	mustbe(jk_rrb);
	mustbe(jk_lcb);
	jn_statements stmt = parseStatements();
	mustbe(jk_rcb);
	return jn_while_create(cond, stmt);
}

jn_if parseIfStmt()
{
	mustbe(jk_if);
	mustbe(jk_lrb);
	jn_expr cond = parseExpresion();
	mustbe(jk_rrb);
	mustbe(jk_lcb);
	jn_statements stmts = parseStatements();
	mustbe(jk_rcb);
	if(!is(jk_else))
		return jn_if_create(cond, stmts);
	mustbe(jk_else);
	mustbe(jk_lcb);
	jn_statements stmts1 = parseStatements();
	mustbe(jk_rcb);
	return to_jn_if(jn_if_else_create(cond, stmts, stmts1));
}

jn_let parseLetStmt()
{
	mustbe(jk_let);
	string name = token.spelling;
	mustbe(jk_identifier);
	jn_var v = lookup(name);
	jn_var v1 = jn_var_create(v->segment, v->name, v->offset, v->type);
	jn_let let = nullptr;
	if(is(jk_lsb))
	{
		mustbe(jk_lsb);
		jn_expr index = parseExpresion();
		mustbe(jk_rsb);
		mustbe(jk_eq);
		jn_expr expr = parseExpresion();
		let = to_jn_let(jn_let_array_create(v1, index, expr));
	}
	else
	{
		mustbe(jk_eq);
		jn_expr expr = parseExpresion();
		let = jn_let_create(v1, expr);
	}
	mustbe(jk_semi);
	return let;
}

jn_do parseDoStmt()
{
	mustbe(jk_do);
	jn_call call = to_jn_call(parseExpresion());
	jn_do stmt = jn_do_create(call);
	mustbe(jk_semi);	
	return stmt;
}

jn_statements parseStatements()
{
	jn_statements stmts = jn_statements_create();
	while(token.token != jk_rcb && token.token != jk_eoi)
	{
		jn_statement stmt = nullptr;
		if(is(jk_return))
			stmt = to_jn_statement(parseReturnStmt());
		else if(is(jk_while))
			stmt = to_jn_statement(parseWhileStmt());		
		else if(is(jk_if))
			stmt = to_jn_statement(parseIfStmt());
		else if(is(jk_let))
			stmt = to_jn_statement(parseLetStmt());
		else if(is(jk_do))
			stmt = to_jn_statement(parseDoStmt());
		else
			exit(0);
		stmts->append(stmt);
	}
	return stmts;
}

jn_subr_body parseSubroutineBody()
{
	jn_var_decs decs = parseVarDeclarations();
	jn_statements stmts = parseStatements();
	return jn_subr_body_create(decs, stmts);
}

jn_subr_decs parseSubroutines()
{
	jn_subr_decs subrs = jn_subr_decs_create();
	while(is(jk_constructor) || is(jk_function) || is(jk_method))
	{
		//constructor, function, method
		string subr_kind = token.spelling;
		nextToken();	
	
		//return value type.
		string vtype = token.spelling;
		mustbe({jk_int, jk_char, jk_void, jk_boolean, jk_identifier});
		//get the function name

		string funcName = token.spelling;
		newFunc(funcName, subr_kind);
		mustbe(jk_identifier);	

		mustbe(jk_lrb);
		jn_param_list params = parseParamList();	
		mustbe(jk_rrb);

		mustbe(jk_lcb);
		jn_subr_body body = parseSubroutineBody();
		mustbe(jk_rcb);

		jn_subr subr = nullptr;
		if(subr_kind == "constructor")
			subr = jn_constructor_create(vtype, funcName, params, body); 
		else if(subr_kind == "function")
			subr = jn_function_create(vtype, funcName, params, body); 
		else
			subr = jn_method_create(vtype, funcName, params, body); 
		subrs->append(subr);
	}
	return subrs;
}

jn_class jack_parser()
{
	tokens = j_tokeniser();
	nextToken();
	jn_class root = parseClass();	
    return root;
}

int main(int argc,char **argv)
{
    // parse a class and print the abstract syntax tree as XML
    jn_print_as_xml(jack_parser(),4) ;
	
    // flush the output and any errors
    print_output() ;
    print_errors() ;
}


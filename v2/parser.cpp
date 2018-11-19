#include <vector>
#include <algorithm>
#include "ast.h"
#include "tokeniser.h"
#include "parser-symbol.h"

using namespace std;

static tokeniser TK;
static Token token;

Class *symbol_table = nullptr;

static void next()
{
	token = TK->next_token();
}

static bool have(vector<TokenKind>expected)
{
	return find(expected.cbegin(), expected.cend(), token.token)!=expected.cend();
}

static Token mustbe(vector<TokenKind>expected)
{
	if(!have(expected))
		exit(0);
	Token old = token;
	next();
	return old;
}

static string segment(string keyword)
{
	if(keyword.compare("field") == 0)
		return "this";
	else if(keyword.compare("var") == 0)
		return "local";
	return "static";
}

static jn_statements parse_stmts();
static jn_expr parse_expresion();

static jn_var_decs parse_var_decs()
{
	jn_var_decs decs = jn_var_decs_create();
	while(have({jk_static, jk_field, jk_var}))
	{
		string keyword = token.spelling;
		next();
		string type = mustbe({jk_int, jk_boolean, jk_identifier, jk_char}).spelling;
		do{
			if(have({jk_comma}))
				next();
			string name = mustbe({jk_identifier}).spelling;
			int offset = symbol_table->nextindex(segment(keyword));
			jn_var_dec var = jn_var_dec_create(segment(keyword), name, offset, type);
			symbol_table->insert(name, type, var->segment, offset);
			decs->append(var);
		}while(have({jk_comma}));
		mustbe({jk_semi});
	}
	return decs;
}

static jn_param_list parse_param_list()
{
	jn_param_list params = jn_param_list_create();
	while(!have({jk_rrb}))
	{
		string type = mustbe({jk_int, jk_char, jk_boolean, jk_identifier}).spelling;
		string name = mustbe({jk_identifier}).spelling;
		int index = symbol_table->nextindex("argument");
		jn_var_dec var = jn_var_dec_create("argument", name, index, type);
		params->append(var);
		symbol_table->insert(name, type, "argument", index);
		if(have({jk_comma}))
			next();
	}
	return params;	
}

static jn_expr parse_constant()
{
	jn_expr result = nullptr;
	switch(token.token)
	{
		case jk_true: result = to_jn_expr(jn_bool_create(true)); break;
		case jk_false: result = to_jn_expr(jn_bool_create(false)); break;
		case jk_this: result = to_jn_expr(jn_this_create()); break;
		case jk_null: result = to_jn_expr(jn_null_create()); break;
		case jk_integerConstant: result = to_jn_expr(jn_int_create(token.ivalue)); break;		
		case jk_stringConstant: result = to_jn_expr(jn_string_create(token.spelling)); break;
	}
	next();
	return result;
}

static jn_array_index parse_array_index(string id)
{
	variable *v = symbol_table->lookup(id);
	jn_var var = jn_var_create(v->segment, v->name, v->offset, v->kind);
	mustbe({jk_lsb});
	jn_expr index = parse_expresion();
	mustbe({jk_rsb});
	return jn_array_index_create(var, index);
}

static jn_expr_list parse_method_call_params()
{
	jn_expr_list ret = jn_expr_list_create();
	while(!have({jk_rrb}))
	{
		jn_expr e = parse_expresion();
		ret->append(e);
		if(have({jk_comma}))
			next();
	}
	return ret;
}

static jn_call parse_function_call(string id)
{
	string className, subrName = id;
	bool method_call, direct_call = false;
	variable *v = symbol_table->lookup(id);
	if(have({jk_stop}))
	{
		className = (v==nullptr) ? id: v->kind;
		next();
		subrName = mustbe({jk_identifier}).spelling;
		method_call = (v != nullptr);
	}
	else
	{
		direct_call = true;
		className = symbol_table->name;
		method_call = symbol_table->ismethod(subrName);
	}
	mustbe({jk_lrb});
	jn_expr_list params = parse_method_call_params();
	mustbe({jk_rrb});
	if(v != nullptr)
	{
		jn_var var = jn_var_create(v->segment, v->name, v->offset, v->kind);
		params->prefix(to_jn_expr(var));
	}
	else if(direct_call)
		params->prefix(to_jn_expr(jn_this_create()));
	return jn_call_create(method_call, className, subrName, params);
}

static jn_expr parse_term()
{
	if(have({jk_true, jk_false, jk_null, jk_this, jk_integerConstant, jk_stringConstant}))
		return parse_constant();
	else if(have({jk_lrb}))
	{
		next();
		jn_expr expr = parse_expresion();
		mustbe({jk_rrb});
		return expr;		
	}
	else if(have({jk_sub, jk_not}))
	{
		string op = mustbe({jk_sub, jk_not}).spelling;
		jn_expr expr = parse_expresion();
		return to_jn_expr(jn_unary_op_create(op, expr));
	}
	string id = mustbe({jk_identifier}).spelling;
	variable *var = symbol_table->lookup(id);
	if(have({jk_stop, jk_lrb}))	 
		return to_jn_expr(parse_function_call(id));
	else if(have({jk_lsb}))
		return to_jn_expr(parse_array_index(id));	
	else
	{
		if(var == nullptr) exit(0);
		return to_jn_expr(jn_var_create(var->segment, var->name, var->offset, var->kind));
	}
}

static jn_expr parse_expresion()
{
	jn_expr left = parse_term();
	jn_expr result = left;
	if(have({jk_add, jk_sub, jk_times, jk_divide, jk_and, jk_or, jk_lt, jk_gt, jk_eq}))
	{
		string op = token.spelling;
		next();
		jn_expr right = parse_expresion();
		result = to_jn_expr(jn_infix_op_create(left, op, right));
	}
	return result;
}


static jn_return parse_return_stmt()
{
	next();
	jn_return ret = nullptr;
	if(have({jk_semi}))
		ret = jn_return_create();
	else
		ret = to_jn_return(jn_return_expr_create(parse_expresion()));
	mustbe({jk_semi});
	return ret;
}

static jn_while parse_while_stmt()
{
	next();
	mustbe({jk_lrb}); 
	jn_expr cond = parse_expresion();
	mustbe({jk_rrb});
	mustbe({jk_lcb});
	jn_statements stmts = parse_stmts();
	mustbe({jk_rcb});
	return jn_while_create(cond, stmts);
}

static jn_if parse_if_stmt()
{
	next();
	mustbe({jk_lrb});
	jn_expr cond = parse_expresion();
	mustbe({jk_rrb});
	mustbe({jk_lcb});
	jn_statements stmts1 = parse_stmts();
	mustbe({jk_rcb});
	if(!have({jk_else}))
		return jn_if_create(cond, stmts1);
	next();
	mustbe({jk_lcb});
	jn_statements stmts2 = parse_stmts();
	mustbe({jk_rcb});
	return to_jn_if(jn_if_else_create(cond, stmts1, stmts2));
}

static jn_let parse_let_stmt()
{
	mustbe({jk_let});
	string id = mustbe({jk_identifier}).spelling;
	variable* v = symbol_table->lookup(id);
	jn_var var = jn_var_create(v->segment, v->name, v->offset, v->kind);
	jn_expr index = nullptr;
	if(have({jk_lsb}))
	{
		mustbe({jk_lsb});
		index = parse_expresion();
		mustbe({jk_rsb});
	}
	mustbe({jk_eq});
	jn_expr expr = parse_expresion();
	mustbe({jk_semi});
	if(index == nullptr)
		return jn_let_create(var, expr);
	return to_jn_let(jn_let_array_create(var, index, expr));
}

static jn_do parse_do_stmt()
{
	mustbe({jk_do});
	jn_call call = parse_function_call(mustbe({jk_identifier}).spelling);
	mustbe({jk_semi});
	return jn_do_create(call);
}

static jn_statements parse_stmts()
{
	jn_statements stmts = jn_statements_create();
	while(have({jk_return, jk_if, jk_let, jk_while, jk_do}))
	{
		jn_statement stmt = nullptr;
		switch(token.token)
		{
			case jk_return: stmt = to_jn_statement(parse_return_stmt()); break;
			case jk_while: stmt = to_jn_statement(parse_while_stmt()); break;
			case jk_if: stmt = to_jn_statement(parse_if_stmt()); break;
			case jk_let: stmt = to_jn_statement(parse_let_stmt()); break;
			case jk_do: stmt = to_jn_statement(parse_do_stmt()); break;
		}
		stmts->append(stmt);
	}
	return stmts;
}

static jn_subr_body parse_subr_body()
{
	mustbe({jk_lcb});
	jn_var_decs localvars = parse_var_decs();
	jn_statements stmts = parse_stmts();
	jn_subr_body body = jn_subr_body_create(localvars, stmts);
	mustbe({jk_rcb});
	return body;
}

static jn_subr_decs parse_subr_decs()
{
	jn_subr_decs subrs = jn_subr_decs_create();
	while(have({jk_constructor, jk_function, jk_method}))
	{
		string keyword = token.spelling;
		next();
		string vtype = mustbe({jk_int, jk_void, jk_char, jk_boolean, jk_identifier}).spelling;
		string name = mustbe({jk_identifier}).spelling;
		symbol_table->addFunction(name, keyword);
		mustbe({jk_lrb});
		jn_param_list params = parse_param_list();
		mustbe({jk_rrb});
		jn_subr_body body = parse_subr_body();
		jn_subr subr = nullptr;
		if(keyword.compare("constructor") == 0)
			subr = to_jn_subr(jn_constructor_create(vtype, name, params, body));
		else if(keyword.compare("function") == 0)
			subr = to_jn_subr(jn_function_create(vtype, name, params, body));
		else
			subr = to_jn_subr(jn_method_create(vtype, name, params, body));
		subrs->append(subr);
	}
	return subrs;
}

static jn_class parse_class()
{
	mustbe({jk_class});
	Token classname = mustbe({jk_identifier});
	symbol_table = new Class(classname.spelling);
	mustbe({jk_lcb});
	jn_var_decs vars = parse_var_decs();
	jn_subr_decs decs = parse_subr_decs();
	mustbe({jk_rcb});
	return jn_class_create(classname.spelling, vars, decs);
}

jn_class jack_parser()
{
	TK = j_tokeniser();
	next();
	jn_class root = parse_class();
	mustbe({jk_eoi});
    return root;
}

int main(int argc,char **argv)
{
    jn_print_as_xml(jack_parser(),4);
    print_output() ;
    print_errors() ;
}


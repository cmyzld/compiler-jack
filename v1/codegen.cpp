#include <iostream>
#include <string>
#include <sstream>
#include <stdio.h>
#include <map>
#include "ast.h"

using namespace std;

static int ifs = 0, whiles = 0;

string int2str(int val)
{
	char *p = new char(20);
	sprintf(p, "%d", val);
	string s = p;
	return s;
}

void write(string key1, string key2, int num)
{
	write_to_output(key1 + " " + key2 + " " + int2str(num) + "\n");
}

void write(string key)
{
	write_to_output(key + "\n");
}

void expresion(jn_expr expr)
{
	map<string, string> infixop = {{"+", "add"}, {"-", "sub"}, {"*", "call Math.multiply 2"},
		{"/", "call Math.divide 2"}, {">", "gt"}, {"<", "lt"}, {"=", "eq"}, {"&", "and"}, {"|", "or"}};
	switch(ast_kind_of(expr))
	{
		case k_jn_bool: 
				write("push", "constant", 0);
				if(to_jn_bool(expr)->tf)
					write("not");
			break;
	
		case k_jn_this:
				write("push", "pointer", 0);		
				break;
			
		case k_jn_null:
				write("push", "constant", 0);
				break;
	
		case k_jn_int:
				write("push", "constant", to_jn_int(expr)->ic);
				break;
	
		case k_jn_string:{
				string val = to_jn_string(expr)->sc;
				write("push", "constant", val.length());
				write("call", "String.new", 1);
				for(int i = 0; i < val.length(); i++)
				{
					write("push", "constant", val[i]);
					write("call", "String.appendChar", 2);
				}
			}break;
		
		case k_jn_unary_op:{
				expresion(to_jn_unary_op(expr)->expr);
				write(to_jn_unary_op(expr)->op == "-" ? "neg": "not");
			}break;
		
		case k_jn_infix_op:{
				jn_infix_op infix = to_jn_infix_op(expr);
				expresion(infix->lhs);
				expresion(infix->rhs);
				write(infixop[infix->op]);
			} break;
	
		case k_jn_var:{
			jn_var var = to_jn_var(expr);
			write("push", var->segment, var->offset);
			} break;

		case k_jn_call: {
				jn_call call = to_jn_call(expr);
				jn_expr_list e = call->expr_list;
				for(int i = 0; i < e->size(); i++)
					expresion(e->get(i));
				write("call", call->class_name + "." + call->subr_name, e->size());
			} break;

		case k_jn_array_index:{
				jn_array_index index = to_jn_array_index(expr);
				expresion(index->index);
				write("push", index->var->segment, index->var->offset);
				write("add");
				write("pop", "pointer", 1);
				write("push", "that", 0);
			} break;
	}
}

void statements(jn_statements stmts)
{
	for(int i = 0; i < stmts->size(); i++)
	{
		jn_statement stmt = stmts->get(i);
		ast_kind k = ast_kind_of(stmt);
		switch(k)
		{
			case k_jn_return: case k_jn_return_expr:
			{
				if(k == k_jn_return_expr)
					expresion(to_jn_return_expr(stmt)->expr);
				else
					write("push", "constant", 0);
				write("return");
			}
			break;		
			case k_jn_while:
			{
				string n = int2str(whiles++);
				jn_while s = to_jn_while(stmt);
				write("label WHILE_EXP" + n);
				expresion(s->cond);
				write("not");
				write("if-goto WHILE_END"+n);
				statements(s->body);
				write("goto WHILE_EXP"+n);
				write("label WHILE_END" + n);
			} break;
			case k_jn_if: case k_jn_if_else:
			{	
				jn_if s = to_jn_if(stmt);
				string truelable = "IF_TRUE" + int2str(ifs); 
				string falselable = "IF_FALSE" + int2str(ifs); 
				string end = "IF_END" + int2str(ifs++);
				expresion(s->cond);
				write("if-goto " + truelable);
				write("goto " + falselable);
				write("label " + truelable);
				statements(s->if_true);
				if(k == k_jn_if)
					write("label " + falselable);
				else
				{
					write("goto " + end);
					write("label " + falselable);
					statements(to_jn_if_else(s)->if_false);
					write("label " + end);
				}
			} break;
			case k_jn_let:
			{
				jn_let let = to_jn_let(stmt);
				expresion(let->expr);
				write("pop", let->var->segment, let->var->offset);
			} break;
			case k_jn_let_array:
			{
				jn_let_array let = to_jn_let_array(stmt);
				expresion(let->index);
				write("push", let->var->segment, let->var->offset);
				write("add");
				expresion(let->expr);
				write("pop", "temp", 0);
				write("pop", "pointer", 1);
				write("push", "temp", 0);
				write("pop", "that", 0);
			} break;
			case k_jn_do:
			{
				expresion(to_jn_expr(to_jn_do(stmt)->call));
				write("pop", "temp", 0);
			}
		}	
	}	
}

void jack_codegen(jn_class t)
{
	if(t != nullptr)
	{
		jn_subr_decs subrs = t->subrs;
		for(int i  = 0; i < subrs->size(); i++)
		{
			jn_subr subr = subrs->get(i);
			jn_var_decs local_vars = subr->body->decs;
			jn_statements body = subr->body->body;
			write("function", t->class_name + "."+ subr->name, local_vars->size());
			if(ast_kind_of(subr) == k_jn_constructor)
			{
				int locals = 0;
				for(int j = 0; j < t->decs->size(); j++)
					if(t->decs->get(j)->segment == "this")
						locals++;
				write("push", "constant", locals);
				write("call", "Memory.alloc", 1);
				write("pop", "pointer", 0);
			}
			else if(ast_kind_of(subr) == k_jn_method)
			{
				write("push", "argument", 0);
				write("pop", "pointer", 0);
			}
			ifs = whiles = 0;
			statements(body);
		}
	}
}

int main(int argc,char **argv)
{
    // parse an AST in XML and print VM code
    jack_codegen(jn_parse_xml()) ;

    // flush the output and any errors
    print_output() ;
    print_errors() ;
}


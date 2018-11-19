#include "ast.h"
#include <string>
#include <sstream>
using namespace std;

void write_indent(int indent)
{
	for(int i = 0; i < indent * 4; i++)
		write_to_output(" ");
}

void write_params(jn_param_list params)
{
	for(int i = 0; i < params->size(); i++)
	{
		jn_var_dec var = params->get(i);
		write_to_output(var->type + " " + var->name);
		if(i < params->size() - 1)
			write_to_output(",");
	}
}

string getkeyword(string segment)
{
	if(segment.compare("this") == 0)
		return "field";
	else if(segment.compare("local") == 0)
		return "var";
	else
		return segment;
}

void write_vardecs(jn_var_decs vars, int indent)
{
	for(int i = 0; i < vars->size(); i++)	
	{
		jn_var_dec var = vars->get(i);	
		write_indent(indent);
		write_to_output(getkeyword(var->segment) + " " + var->type + " " + var->name + " ;\n");
	}
}

void write_bool_expr(jn_bool b)
{
	write_to_output(b->tf?"true":"false");
}


void write_expresion(jn_expr expr);
void write_expr_list(jn_expr_list list, int start)
{
	for(int i = start; i < list->size(); i++)
	{
		write_expresion(list->get(i));
		if(i < list->size() - 1)
			write_to_output(",");
	}
}

void write_expresion(jn_expr expr)
{
	ast_kind kind = ast_kind_of(expr);
	switch(kind)
	{
		case k_jn_bool: write_bool_expr(to_jn_bool(expr)); break;
		case k_jn_var: write_to_output(to_jn_var(expr)->name); break;
		case k_jn_this: write_to_output("this"); break;
		case k_jn_null: write_to_output("null"); break;
		case k_jn_int:
			{
				stringstream ss;
				ss << to_jn_int(expr)->ic;
				write_to_output(ss.str()); 
			}
			break;
		case k_jn_string:
				write_to_output("\"" + to_jn_string(expr)->sc + "\"");
				break;
		case k_jn_unary_op:
			{
				jn_unary_op unary = to_jn_unary_op(expr);
				write_to_output(unary->op);
				write_expresion(unary->expr);
			}
			break;
		case k_jn_infix_op:
			{
				jn_infix_op exp = to_jn_infix_op(expr);
				write_expresion(exp->lhs);
				write_to_output(" " + exp->op + " ");
				write_expresion(exp->rhs);
			}
			break;
		case k_jn_call:
			{
				jn_call call = to_jn_call(expr);
				string method("");		
				if(call->method_call && ast_kind_of(call->expr_list->get(0)) == k_jn_this)
					method = "this.";
				else
					method = (call->method_call ? to_jn_var(call->expr_list->get(0))->name: call->class_name) + ".";
				write_to_output(method + call->subr_name + "(");
				write_expr_list(call->expr_list, call->method_call ? 1 : 0);
				write_to_output(")");
			}
			break;
		case k_jn_array_index:
			{
				jn_array_index exp = to_jn_array_index(expr);
				write_to_output(exp->var->name + "[");
				write_expresion(exp->index);
				write_to_output("]");
			}
			break;
	}
}


void write_statements(jn_statements stmts, int indent);
void write_while(jn_while stmt, int indent)
{
	write_indent(indent);
	write_to_output("while (");
	write_expresion(stmt->cond);
	write_to_output(")\n");
	write_indent(indent);
	write_to_output("{\n");
	write_statements(stmt->body, indent + 1);
	write_indent(indent);
	write_to_output("}\n");
}

void write_if(jn_if stmt, int indent)
{
	write_indent(indent);
	write_to_output("if (");
	write_expresion(stmt->cond);
	write_to_output(")\n");
	write_indent(indent);
	write_to_output("{\n");
	write_statements(stmt->if_true, indent+1);
	write_indent(indent);
	write_to_output("}\n");
	if(ast_kind_of(stmt) == k_jn_if_else)
	{
		write_indent(indent);
		write_to_output("else\n");
		write_indent(indent);
		write_to_output("{\n");
		write_statements(to_jn_if_else(stmt)->if_false, indent+1);
		write_indent(indent);
		write_to_output("}\n");
	}
}

void write_let(jn_let stmt, int indent)
{
	write_indent(indent);
	write_to_output("let ");
	write_to_output(stmt->var->name);
	if(ast_kind_of(stmt) == k_jn_let_array)
	{
		write_to_output("[");
		write_expresion(to_jn_let_array(stmt)->index);
		write_to_output("]");
	}
	write_to_output(" = ");
	write_expresion(stmt->expr);
	write_to_output(" ;\n");
}

void write_statements(jn_statements stmts, int indent)
{
	for(int i = 0; i < stmts->size(); i++)
	{
		jn_statement stmt = stmts->get(i);
		ast_kind kind = ast_kind_of(stmt);
		switch(kind)
		{
			case k_jn_return: case k_jn_return_expr:
				write_indent(indent);
				write_to_output("return");
				if(kind == k_jn_return_expr)
				{
					write_to_output(" ");
					write_expresion(to_jn_return_expr(stmt)->expr);
				}
				write_to_output(" ;\n"); 
				break;
			case k_jn_while: 
				write_while(to_jn_while(stmt), indent); 
				if(i < stmts->size() - 1)
					write_to_output("\n");
				break;
			case k_jn_if: case k_jn_if_else:
				write_if(to_jn_if(stmt), indent);
				if(i < stmts->size() - 1)
					write_to_output("\n");
				break;
			case k_jn_let: case k_jn_let_array:
				write_let(to_jn_let(stmt), indent);
				break;
			case k_jn_do:
				write_indent(indent);
				write_to_output("do ");
				write_expresion(to_jn_expr(to_jn_do(stmt)->call));
				write_to_output(" ;\n");
		}
	}	
}

void write_subroutines(jn_subr_decs subrs, string className, int indent)
{
	for(int i = 0; i < subrs->size(); i++)
	{
		jn_subr subr = subrs->get(i);
		ast_kind kind = ast_kind_of(subr);
		write_indent(indent);
		switch(kind)
		{
			case k_jn_function: write_to_output("function "); break;
			case k_jn_method: write_to_output("method "); break;
			case k_jn_constructor: write_to_output("constructor "); break;
		}
		write_to_output(subr->vtype + " " + className + "." + subr->name + "(");
		write_params(subr->params);
		write_to_output(")\n");
		write_indent(indent);
		write_to_output("{\n");
		write_vardecs(subr->body->decs, indent + 1);
		if(subr->body->decs->size() != 0)
			write_to_output("\n");
		write_statements(subr->body->body, indent + 1);
		write_indent(indent);
		write_to_output("}\n");
		if(i < subrs->size() - 1)
			write_to_output("\n");
	}
}

void print_class(jn_class t, int indent)
{
	write_to_output("class " + t->class_name + "\n");
	write_to_output("{\n");
	write_vardecs(t->decs, indent + 1);
	if(t->decs->size() != 0)
		write_to_output("\n");
	write_subroutines(t->subrs, t->class_name, indent + 1);
	write_to_output("}\n");
}

void jack_pretty(jn_class t)
{
	print_class(t, 0);
}

int main(int argc,char **argv)
{
    // parse an AST in XML and pretty print as Jack
    jack_pretty(jn_parse_xml()) ;

    // flush the output and any errors
    print_output() ;
    print_errors() ;
}


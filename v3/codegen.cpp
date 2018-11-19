#include "ast.h"
#include <sstream>
#include <map>

using namespace std;

int numif = 0, numwhile = 0;

map<ast_kind, void(*)(jn_expr)>exprmap;
map<ast_kind, void(*)(jn_statement)>smap;
map<string, string>infixmap = {
	{"+", "add"}, {"-","sub"}, {"*", "call Math.multiply 2"}, {"/", "call Math.divide 2"},
	{">", "gt"}, {"<", "lt"}, {"=", "eq"}, {"&", "and"}, {"|", "or"}};
string str(int value)
{
	stringstream ss;
	ss << value;
	return ss.str();
}

void write1(string k){ write_to_output(k + "\n"); }
void write2(string k1, string k2){ write_to_output(k1+" " + k2+"\n");}
void write3(string k1, string k2, int k3) { write_to_output(k1+" "+k2+" "+ str(k3)+"\n");}
void t_int(jn_expr e){ write3("push", "constant", to_jn_int(e)->ic);}

void t_bool(jn_expr e)
{
	write3("push", "constant", 0);
	if(to_jn_bool(e)->tf)
		write1("not");
}

void t_string(jn_expr e)
{
	string val = to_jn_string(e)->sc;
	write3("push", "constant", val.length());
	write3("call", "String.new", 1);
	for(auto i = 0; i < val.length(); i++)
	{
		write3("push", "constant", val[i]);
		write3("call", "String.appendChar", 2);
	}
}

void t_null(jn_expr e){ write3("push", "constant", 0); }
void t_this(jn_expr e) { write3("push", "pointer", 0); }
void t_var(jn_expr e){ write3("push", to_jn_var(e)->segment, to_jn_var(e)->offset);}

void translate_expresion(jn_expr);
void t_array(jn_expr e)
{
	jn_array_index j = to_jn_array_index(e);
	translate_expresion(j->index);
	write3("push", j->var->segment, j->var->offset);
	write1("add");
	write3("pop", "pointer", 1);
	write3("push", "that", 0);
}

void t_unary(jn_expr e)
{
	translate_expresion(to_jn_unary_op(e)->expr);
	write1(to_jn_unary_op(e)->op == "-"?"neg":"not");	
}

void t_infix(jn_expr e)
{
	jn_infix_op o = to_jn_infix_op(e);
	translate_expresion(o->lhs);
	translate_expresion(o->rhs);	
	write1(infixmap[o->op]);	
}

void t_call(jn_expr e)
{
	jn_call call = to_jn_call(e);
	for(auto i = 0; i < call->expr_list->size(); i++)
		translate_expresion(call->expr_list->get(i));
	write3("call", call->class_name+"."+call->subr_name, call->expr_list->size());	
}

map<ast_kind, void(*)(jn_expr)>_exprmap = {
	{k_jn_int, t_int}, {k_jn_string, t_string}, {k_jn_bool, t_bool}, {k_jn_null, t_null},
	{k_jn_this, t_this}, {k_jn_var, t_var}, {k_jn_array_index, t_array}, 
	{k_jn_unary_op, t_unary}, {k_jn_infix_op, t_infix}, {k_jn_call, t_call}};

void translate_expresion(jn_expr expr)
{
	exprmap[ast_kind_of(expr)](expr);
}

void t_return(jn_statement s)
{
	if(ast_kind_of(s) == k_jn_return)
		write3("push", "constant", 0);
	else
		translate_expresion(to_jn_return_expr(s)->expr);
	write1("return");
}

void t_let(jn_statement s)
{
	jn_let l = to_jn_let(s);
	translate_expresion(l->expr);
	write3("pop", l->var->segment, l->var->offset);		
}

void t_let_array(jn_statement s)
{
	jn_let_array a = to_jn_let_array(s);
	translate_expresion(a->index);
	write3("push", a->var->segment, a->var->offset);
	write1("add");
	translate_expresion(a->expr);
	write3("pop", "temp", 0);
	write3("pop", "pointer", 1);
	write3("push", "temp", 0);
	write3("pop", "that", 0);
}

void translate_statements(jn_statements);
void t_if(jn_statement s)
{
	translate_expresion(to_jn_if(s)->cond);
	int n = numif++;
	write2("if-goto", "IF_TRUE"+str(n));
	write2("goto", "IF_FALSE"+str(n));
	write2("label", "IF_TRUE"+str(n));
	translate_statements(to_jn_if(s)->if_true);
	if(ast_kind_of(s) == k_jn_if)
		write2("label", "IF_FALSE"+str(n));
	else
	{
		write2("goto", "IF_END"+str(n));
		write2("label", "IF_FALSE"+str(n));
		translate_statements(to_jn_if_else(s)->if_false);
		write2("label", "IF_END"+str(n));
	}
}

void t_while(jn_statement s)
{
	string n = str(numwhile++);
	write2("label", "WHILE_EXP"+n);
	translate_expresion(to_jn_while(s)->cond);
	write1("not");
	write2("if-goto", "WHILE_END"+n);
	translate_statements(to_jn_while(s)->body);
	write2("goto", "WHILE_EXP"+n);
	write2("label", "WHILE_END"+n);
}

void t_do(jn_statement s)
{
	translate_expresion(to_jn_expr(to_jn_do(s)->call));
	write3("pop", "temp", 0);
}

map<ast_kind, void(*)(jn_statement)>_smap = {
	{k_jn_return, t_return}, {k_jn_return_expr, t_return}, {k_jn_let, t_let}, {k_jn_let_array, t_let_array},
	{k_jn_do, t_do}, {k_jn_if, t_if}, {k_jn_if_else, t_if}, {k_jn_while, t_while}
};

void translate_statements(jn_statements s)
{
	for(auto i = 0; i < s->size(); i++)
		smap[ast_kind_of(s->get(i))](s->get(i));	
}

void translate_class(jn_class t)
{
	if(!t) return;
	for(auto i = 0; i < t->subrs->size(); i++)
	{
		auto subr = t->subrs->get(i);
		write3("function", t->class_name + "."+subr->name, subr->body->decs->size());
		if(ast_kind_of(subr) == k_jn_method)
		{
			write3("push", "argument", 0);
			write3("pop", "pointer", 0);
		}	
		if(ast_kind_of(subr) == k_jn_constructor)
		{
			int fields_count = 0;
			for(auto j = 0; j < t->decs->size(); j++)
				if(t->decs->get(j)->segment == "this")
					fields_count++;
			write3("push", "constant", fields_count);
			write3("call", "Memory.alloc", 1);
			write3("pop", "pointer", 0);
		}
		numif = numwhile = 0;
		translate_statements(subr->body->body);
	}
}

void jack_codegen(jn_class t)
{
	exprmap = _exprmap;
	smap = _smap;
	translate_class(t);
}

int main(int argc,char **argv)
{
    jack_codegen(jn_parse_xml()) ;
    print_output() ;
    print_errors() ;
}


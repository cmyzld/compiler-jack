#include "ast.h"
#include <map>
#include <sstream>

using namespace std;

string str(int value)
{
	stringstream ss;
	ss << value;
	return ss.str();
}

void indent(int);
void out(string s, int d = -1)
{
	if(d != -1)
		indent(d);
	write_to_output(s);
}

void out1(string s, int d = -1)
{
	if(d != -1)
		indent(d);
	write_to_output(s+"\n");
}

void out2(string k1, string k2)
{
	out(k1+" "+k2+"\n");
}

void out3(string k1, string k2, string k3)
{
	out(k1+" " + k2+" "+k3+"\n");
}

void out4(string k1, string k2, string k3, string k4)
{
	out(k1+" "+k2+" "+k3+" "+k4+"\n");
}

void indent(int i)
{
	for(auto j = 0; j < i * 4; j++)
		out(" ");
}

void w_int(jn_expr e)
{
	out(str(to_jn_int(e)->ic));		
}

void w_bool(jn_expr e)
{
	out(to_jn_bool(e)->tf ? "true": "false");	
}

void w_string(jn_expr e)
{
	out("\""+to_jn_string(e)->sc+"\"");
}

void w_null(jn_expr e)
{
	out("null");
}

void w_this(jn_expr e)
{
	out("this");
}

void w_var(jn_expr e)
{
	out(to_jn_var(e)->name);
}

void w_expresion(jn_expr);
void w_array(jn_expr e)
{
	auto a = to_jn_array_index(e);
	out(a->var->name+"[");
	w_expresion(a->index);
	out("]");
}

void w_unary(jn_expr e)
{
	out(to_jn_unary_op(e)->op);
	w_expresion(to_jn_unary_op(e)->expr);	
}

void w_infix(jn_expr e)
{
	auto  o = to_jn_infix_op(e);
	w_expresion(o->lhs);
	out(" " + o->op + " ");
	w_expresion(o->rhs);
}

void w_call(jn_expr e)
{
	jn_call c = to_jn_call(e);
	string method;
	if(c->method_call && ast_kind_of(c->expr_list->get(0)) == k_jn_this)
		method = "this";
	else 
		method = (c->method_call?to_jn_var(c->expr_list->get(0))->name: c->class_name);
	out(method +"."+ c->subr_name+"(");
	int start = c->method_call?1:0;
	for(int i = start; i < c->expr_list->size(); i++)
	{
		w_expresion(c->expr_list->get(i));
		if(i!=c->expr_list->size() - 1)
			out(",");
	}
	out(")");	
}

map<ast_kind, void(*)(jn_expr)>emap = {
	{k_jn_int, w_int}, {k_jn_string, w_string}, {k_jn_bool, w_bool}, {k_jn_null, w_null},
	{k_jn_this, w_this}, {k_jn_var, w_var}, {k_jn_array_index, w_array}, 
	{k_jn_unary_op, w_unary}, {k_jn_infix_op, w_infix}, {k_jn_call, w_call}};

void w_expresion(jn_expr e)
{
	emap[ast_kind_of(e)](e);
}

void w_statements(jn_statements, int);
void w_while(jn_statement s, int d)
{
	indent(d);
	out("while (");
	w_expresion(to_jn_while(s)->cond);
	out1(")");
	out1("{", d);
	w_statements(to_jn_while(s)->body, d + 1);	
	out1("}", d);
}

//for if and if-else
void w_if(jn_statement s, int d)
{
	indent(d);
	out("if (");
	w_expresion(to_jn_if(s)->cond);
	out1(")");
	out1("{", d);
	w_statements(to_jn_if(s)->if_true, d+1);
	out1("}", d);
	if(ast_kind_of(s) == k_jn_if_else)
	{
		out1("else", d);
		out1("{", d);
		w_statements(to_jn_if_else(s)->if_false, d+1);
		out1("}", d);
	}
}

void w_let(jn_statement s, int d)
{
	out("let ", d);
	out(to_jn_let(s)->var->name);
	if(ast_kind_of(s) == k_jn_let_array)
	{
		out("[");
		w_expresion(to_jn_let_array(s)->index);
		out("]");
	}
	out(" = ");
	w_expresion(to_jn_let(s)->expr);
	out1(" ;");
}

void w_do(jn_statement s, int d)
{
	out("do ", d);
	w_expresion(to_jn_expr(to_jn_do(s)->call));
	out1(" ;");
}

//for return and return-expr
void w_return(jn_statement s, int d)
{
	indent(d);
	out("return");
	if(ast_kind_of(s) == k_jn_return_expr)
	{
		out(" ");
		w_expresion(to_jn_return_expr(s)->expr);
	}
	out1(" ;");
}

map<ast_kind, void(*)(jn_statement, int)>smap = {
	{k_jn_return, w_return}, {k_jn_return_expr, w_return}, {k_jn_let, w_let}, {k_jn_let_array, w_let},
	{k_jn_do, w_do}, {k_jn_if, w_if}, {k_jn_if_else, w_if}, {k_jn_while, w_while}
};

void w_statements(jn_statements s, int d)
{
	for(auto i = 0; i < s->size(); i++)
	{
		smap[ast_kind_of(s->get(i))](s->get(i), d);	
		ast_kind k = ast_kind_of(s->get(i));
		if((k == k_jn_if || k == k_jn_if_else || k == k_jn_while) && i != s->size() - 1)
			out1("");
	}
}

void w_vars(jn_var_decs v, int d)
{
	for(auto i = 0; i < v->size(); i++)
	{
		indent(d);
		jn_var_dec j = v->get(i);
		string keyword = (j->segment == "this")?"field":(j->segment == "local"?"var":"static");
		out4(keyword, j->type, j->name, ";");
	}
}

void w_class(jn_class t, int d)
{
	out2("class", t->class_name);
	out1("{");
	w_vars(t->decs, d+1);
	if(t->decs->size() >= 1)
		out1("");
	for(auto i = 0; i < t->subrs->size(); i++)
	{
		jn_subr r = t->subrs->get(i);
		indent(d + 1);	
		ast_kind k = ast_kind_of(r);
		out(k == k_jn_method?"method":(k == k_jn_function?"function":"constructor"));
		out(" "+ r->vtype+" "+t->class_name+"."+r->name+"(");
		for(auto j = 0; j < r->params->size(); j++)
		{
			jn_var_dec v = r->params->get(j);
			out(v->type+" "+v->name);
			if(j != r->params->size()-1)
				out(",");
		}
		out1(")");
		indent(d + 1);
		out1("{");
		w_vars(r->body->decs, d+2);
		if(r->body->decs->size() > 0)
			out1("");
		w_statements(r->body->body, d+2);
		indent(d+ 1);
		out1("}");
		if(i != t->subrs->size() - 1)
			out1("");
	}
	out1("}");
}

void jack_pretty(jn_class t)
{
	w_class(t, 0);
}

int main(int argc,char **argv)
{
    // parse an AST in XML and pretty print as Jack
    jack_pretty(jn_parse_xml()) ;

    // flush the output and any errors
    print_output() ;
    print_errors() ;
}


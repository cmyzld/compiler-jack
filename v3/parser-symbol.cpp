#include "parser-symbol.h"

#include <iostream>

func::func(string n, string t)
	:name(n), type(t), local_index(0)
{
	vars = create_symbol_table_values();
	argument_index = (t.compare("method") == 0) ? 1 : 0;
}

Class::Class(string n):name(n), static_index(0), this_index(0)
{
	vars = create_symbol_table_values();
}

void Class::insert(string n, string k, string seg, int offset)
{
	variable *var = new variable(n, k, seg, offset);
	if(seg.compare("this") == 0 || seg.compare("static") == 0)
		vars->insert(n, var);
	else
		current_function->insert(var);
}

void func::insert(variable *var)
{
	vars->insert(var->name, var);
}

int func::nextindex(string segment)
{
	if(segment.compare("local") == 0)
		return local_index++;
	return argument_index++;
}

int Class::nextindex(string segment)
{
	if(segment.compare("static") == 0)
		return static_index++;
	else if(segment.compare("this") == 0)
		return this_index++;
	return current_function->nextindex(segment);
}

void Class::addFunction(string n, string t)
{
	func *f = new func(n, t);
	functions.push_back(f);
	current_function = f;
}

variable* func::lookup(string n)
{
	return (variable *)(vars->lookup(n));
}

variable* Class::lookup(string n)
{
	variable* v = (variable *)(vars->lookup(n));
	if(v != nullptr)
		return v;
	return current_function->lookup(n);
}

bool Class::ismethod(string funcname)
{
	for(func* f: functions)
		if(funcname.compare(f->name) == 0)
			return f->type.compare("method") == 0;
	return true;
}

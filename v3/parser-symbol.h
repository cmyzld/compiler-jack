#include <string>
#include <vector>
#include "symbols.h"

using namespace std;

class variable: public symbol_value
{
	public:
		string name, kind, segment;
		int offset;
		variable(string m, string k, string s, int o)
			:name(m), kind(k), segment(s), offset(o){}
};

class func
{
public:
	string name, type;
	symbol_table_values vars;		
	func(string name, string type);
	void insert(variable *var);
	variable* lookup(string n);
	int nextindex(string segment);
private:
	int argument_index, local_index;
};

struct Class
{
	string name;
	vector<func*>functions;
	symbol_table_values vars;		

	Class(string name);	
	void insert(string name, string kind, string segment, int offset);
	int nextindex(string type);
	void addFunction(string name, string type);	
	variable* lookup(string n);
	bool ismethod(string funcname);
private:
	func *current_function;
	int static_index, this_index;
};

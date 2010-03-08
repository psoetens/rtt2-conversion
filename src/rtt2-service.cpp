//============================================================================
// Name        : rtt2-converter.cpp
// Author      : Peter Soetens <peter@thesourceworks.com>
// Version     :
// Copyright   : (c) The SourceWorks
// Description : Creates a ServiceProvider from an interface.
//============================================================================

#include <iostream>
#include <boost/bind.hpp>
#include "boost/spirit/include/qi.hpp"
#include "boost/spirit/include/phoenix_core.hpp"
#include "boost/spirit/include/phoenix_operator.hpp"

#include <boost/spirit/include/qi_match.hpp>
#include <boost/spirit/include/qi_stream.hpp>
#include <boost/spirit/include/qi_raw.hpp>
#include <boost/spirit/home/phoenix/bind/bind_function.hpp>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/tuple.hpp>

#include <ostream>
#include <iostream>
#include <fstream>
#include <string>
#include <functional>

using namespace std;

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;
namespace bf = boost::fusion;

//using namespace qi;
//using namespace ascii;
using qi::char_;
using qi::lexeme;
using qi::alpha;
using qi::lit;

// All we need to generate a function signature.
struct funcsig {
    string name;
    string ret;
    vector<string> args;
};

template <typename Iterator>
struct converter : qi::grammar<Iterator,method_reg(), ascii::space_type>
{

    converter() : converter::base_type(start)
    {
        mfunction = -lit("virtual") >> qtype >> ident >> '(' >> -(marg >> *(',' >> marg)) >> ')' >> -lit("const") >> -('=' >> '0') >> ';';
        marg = qtype >> ident >> -(lit("=") >> initarg);
        qtype = -lit("const") >> type >> -lit("const") >> -lit("&");
        type = -lit("::") >> ident >> *(lit("::") >> ident);
        initarg = *(char_ -','); // does not support a = A<B,C>() or a = make_pair(b,c) or ... -- we only parse uptil the first comma.

        // utility parsers:
        quoted = lexeme['"'>> *(char_ - '"') >> '"'];
        identchar = char_("a-zA-Z_0-9");
        ident = alpha >> *char_("a-zA-Z_0-9");
    }

    void found_reg(method_reg m) {
//        cout << "found reg:" << m <<endl;
        method = m;
    }

    void found_some() {
//        cout << "Found some !" <<endl;
        method = method_reg();
    }

    void found_method(boost::iterator_range<Iterator> m) {
        std::string s(m.begin(), m.end());
//        cout << "Found method name :" << s <<endl;
        method.name = s;
    }

    void found_command(boost::iterator_range<Iterator> m) {
        std::string s(m.begin(), m.end());
//        cout << "Found method name :" << s <<endl;
        method.is_command=true;
        method.name = s;
    }

    void found_object(boost::iterator_range<Iterator> m) {
        std::string s(m.begin(), m.end());
//        cout << "Found method name :" << s <<endl;
        method.is_object=true;
        iface_objects.push_back( s );
        method.name = s;
    }

    void found_function(boost::iterator_range<Iterator> m) {
        std::string s(m.begin(), m.end());
//        cout << "Found C/C++ function :" << s <<endl;
        method.function = s;
    }

    void found_completion(boost::iterator_range<Iterator> m) {
        std::string s(m.begin(), m.end());
//        cout << "Found C/C++ function :" << s <<endl;
        method.completion = s;
    }

    void found_caller(boost::iterator_range<Iterator> m) {
        std::string s(m.begin(), m.end());
//        cout << "Found C++ caller :" << s <<endl;
        method.caller = s;
    }

    void found_desc(boost::iterator_range<Iterator> m) {
        std::string s(m.begin(), m.end());
//        cout << "Found description :" << s <<endl;
        method.description = s;
    }

    void found_arg(boost::iterator_range<Iterator> m) {
        std::string s(m.begin(), m.end());
//        cout << "Found arg :" << s <<endl;
        method.args.push_back( arg_desc_type(s,"") );
    }

    void found_arg_desc(boost::iterator_range<Iterator> m) {
        std::string s(m.begin(), m.end());
//        cout << "Found arg desc :" << s <<endl;
        bf::get<1>(method.args.back()) = s;
    }

    void parsed_text(std::vector<char> m) {
//        cout << "Found text :" << (char*)m.data() <<endl;
    }

    // rules return a string and use the space_type as skip parser.
    typedef qi::rule<Iterator, ascii::space_type> rule_t;
    typedef qi::rule<Iterator, std::string(void), ascii::space_type> crule_t;
    typedef qi::rule<Iterator, std::string(void), ascii::space_type> srule_t;
    typedef qi::rule<Iterator, method_reg(void), ascii::space_type> mrule_t;
    rule_t identchar;
    crule_t ident;
    mrule_t start;
    rule_t junk;
    srule_t quoted;
    rule_t addmethod;
    rule_t addmethodds;
    rule_t method_match;
    rule_t methodds_match;
    srule_t method_name;
    rule_t method_caller;
    rule_t method_function;
    rule_t description;
    rule_t arg_name;
    rule_t arg_description;

    srule_t command_name;
    rule_t addcommand;
    rule_t command_match;
    rule_t completion_function;

    rule_t object_match;
    rule_t addobject;
    srule_t object_name;

    method_reg method;
};

#include <boost/regex.hpp>

std::string create_operation(const method_reg& m)
{
    // do only for object reg:
    if ( m.is_object) {
        std::string r = "addOperation( &" + m.name +" )";
        if ( !m.description.empty() )
            r += ".doc(" +  m.description +")";
        for(args_desc::const_iterator it = m.args.begin(); it != m.args.end(); ++it)
            r += ".arg(" + bf::get<0>(*it) + ", " + bf::get<1>(*it) + ")";
        r += ";";
        return r;
    }

    // do for both method and command:
    std::string r = "addOperation(" + m.name +", " + m.function + (!m.caller.empty() ? (", "+m.caller) : "") + (m.is_command ? ", RTT::OwnThread" : ", RTT::ClientThread") + ")";
    if ( !m.description.empty() )
        r += ".doc(" +  m.description +")";
    for(args_desc::const_iterator it = m.args.begin(); it != m.args.end(); ++it)
        r += ".arg(" + bf::get<0>(*it) + ", " + bf::get<1>(*it) + ")";
    r += ";";

    // do only for command:
    if (m.is_command) {
        // add completion condition too.
        r += "\n";
        r += "addOperation(\"" + m.name.substr(1,m.name.length()-2) + "Done\"" +", " + m.completion + (!m.caller.empty() ? (", "+m.caller) : "") + ", RTT::ClientThread" + ")";
        if ( !m.description.empty() )
            r += ".doc(\"Returns true when " +m.name.substr(1,m.name.length()-2)+" is done.\")";
        for(args_desc::const_iterator it = m.args.begin(); it != m.args.end(); ++it)
            r += ".arg(" + bf::get<0>(*it) + ", " + bf::get<1>(*it) + ")";
        r += ";";
    }
    return r;
}

std::string create_operationDS(const method_reg& m){
    std::string r = "addOperationDS(" + m.name +", " + m.function + ',' + m.caller + ")";
    if ( !m.description.empty() )
        r += ".doc(" +  m.description +")";
    for(args_desc::const_iterator it = m.args.begin(); it != m.args.end(); ++it)
        r += ".arg(" + bf::get<0>(*it) + ", " + bf::get<1>(*it) + ")";
    r += ";";
    return r;
}

// a regexp filters out statements to be processed by the parser
template<class Parser>
string process_statement(Parser& p, const std::string& regx, const std::string& text)
{
    boost::regex e(regx);
    boost::smatch what;
    //std::cout << "Expression:  \"" << regx << "\"\n";
    std::string::const_iterator start, end;

    start = text.begin();
    end = text.end();
    std::vector<bf::tuple<std::string,method_reg> > matches;
    while (boost::regex_search(start, end, what, e))
    {
        unsigned i;
        std::cout << "** Match found **\n";
        for (i = 1; i < what.size(); ++i) {
            std::cout << "      $" << i << " = \"" << what[i] << "\"\n";
        }
        std::string input = what[0];
        std::string::const_iterator iter = input.begin();
        std::string::const_iterator end = input.end();
        method_reg m;
        bool r = phrase_parse(iter, end, p, ascii::space, m);
        if (r == false) {
            cout << "parsing of "<< what[0] << " failed" <<endl;
        } else {
//            cout << "parsing of "<< what[0] << " succeeded." <<endl;
//            std::cout << boost::fusion::tuple_open('[');
//             std::cout << boost::fusion::tuple_close(']');
//             std::cout << boost::fusion::tuple_delimiter(", ");
//
//             std::cout << "-------------------------\n";
             std::cout << "Parsing succeeded\n";
////             std::cout << "got: " << boost::fusion::as_vector(m) << std::endl;
             std::cout << "got: " << p.method << std::endl;
//             std::cout << "\n-------------------------\n";
             std::cout << "replace with: " << create_operation(p.method) << endl;
             matches.push_back( bf::tuple<std::string,method_reg>(input,p.method));
        }
        start = what[0].second;
    }
    std::string result = text;
    for(std::vector<bf::tuple<std::string,method_reg> >::iterator it = matches.begin(); it!=matches.end(); ++it) {
        //cout << " match '"<< bf::get<0>(*it) <<"' at : " << result.find(bf::get<0>(*it)) <<endl;
        if ( bf::get<1>(*it).is_ds)
            result = result.replace( result.find(bf::get<0>(*it)), bf::get<0>(*it).length() , create_operationDS( bf::get<1>(*it) ) );
        else
            result = result.replace( result.find(bf::get<0>(*it)), bf::get<0>(*it).length() , create_operation( bf::get<1>(*it) ) );
    }
    //cout << "Final file is:" << endl;
    //cout << result;
    return result;
}

int main(int argc, char** argv) {
    if (argc != 2)
        return -1;
	//cout << "Opening file..." << endl;

	ifstream file( argv[1] );
    if ( !file ) {
        cout << "Error: " << argv[1] << " does not exist !"<<endl;
        return 1;
    }
    string input;
    file.unsetf( ios_base::skipws );
    istream_iterator<char> streambegin( file );
    istream_iterator<char> streamend;
    copy( streambegin, streamend, back_inserter( input ) );

	typedef std::string::const_iterator iterator_type;
	typedef converter<iterator_type> convert_grammar;

	convert_grammar g;

	std::string parsed = input;

    //cout << "file was:" <<endl << input <<endl;
	file.close();
	parsed = process_statement(g,"methods\\(\\).*?;", parsed);
	parsed = process_statement(g,"addMethod\\(.*?;", parsed);

    parsed = process_statement(g,"commands\\(\\).*?;", parsed);
    parsed = process_statement(g,"addCommand\\(.*?;", parsed);

    // also replace TaskObject and OperationInterface to ServiceProvider:
    boost::regex tosp("TaskObject");
    boost::regex oisp("OperationInterface");

    parsed = boost::regex_replace(parsed,tosp,"ServiceProvider");
    parsed = boost::regex_replace(parsed,oisp,"ServiceProvider");
    parsed = boost::regex_replace(parsed, boost::regex("getObject"),"provides");
    parsed = boost::regex_replace(parsed, boost::regex("getMethod"),"getOperation");
    parsed = boost::regex_replace(parsed, boost::regex("Command.hpp"),"Method.hpp");
    parsed = boost::regex_replace(parsed, boost::regex("getCommand"),"getOperation");
    //parsed = boost::regex_replace(parsed, boost::regex("getCommand"),"getMethod");
    parsed = boost::regex_replace(parsed, boost::regex("addObject"),"addService");
    parsed = boost::regex_replace(parsed, boost::regex("removeObject"),"removeService");
    parsed = boost::regex_replace(parsed, boost::regex("internal/ServiceProvider"),"interface/ServiceProvider");
    parsed = boost::regex_replace(parsed, boost::regex("internal::ServiceProvider"),"interface::ServiceProvider");
    parsed = boost::regex_replace(parsed, boost::regex("->methods\\(\\)"),"");
    parsed = boost::regex_replace(parsed, boost::regex("methods\\(\\)->"),"");
    parsed = boost::regex_replace(parsed, boost::regex("->commands\\(\\)"),"");
    parsed = boost::regex_replace(parsed, boost::regex("commands\\(\\)->"),"");
    parsed = boost::regex_replace(parsed, boost::regex("->attributes\\(\\)"),"");
    parsed = boost::regex_replace(parsed, boost::regex("attributes\\(\\)->"),"");
    parsed = boost::regex_replace(parsed, boost::regex("MethodRepository::Factory"),"ServiceProvider");

    // replace all methods/command objects in an interface with the Operation type:
    for(std::vector<std::string>::iterator it = iface_objects.begin(); it != iface_objects.end(); ++it) {
        parsed = boost::regex_replace(parsed, boost::regex("\\bMethod<(.*?)"+*it),"Operation<\\1"+*it);
        parsed = boost::regex_replace(parsed, boost::regex("\\bCommand<(.*?)"+*it),"Operation<\\1"+*it);
    }
    // replace all command objects for calling to method objects:
    parsed = boost::regex_replace(parsed, boost::regex("(\\b)Command<"),"\\1Method<");
    ofstream ofile( argv[1] );
    ofile << parsed;
	return 0;
}

//============================================================================
// Name        : rtt2-converter.cpp
// Author      : Peter Soetens <peter@thesourceworks.com>
// Version     :
// Copyright   : (c) The SourceWorks
// Description : Converts appliation code from RTT 1.x to RTT 2.x syntax
//============================================================================

#include <iostream>
#include <boost/bind.hpp>
#include "boost/spirit/include/qi.hpp"
#include "boost/spirit/include/phoenix_core.hpp"
#include "boost/spirit/include/phoenix_operator.hpp"

#include "boost/spirit/include/qi_match.hpp"
#include "boost/spirit/include/qi_stream.hpp"
#include "boost/spirit/include/qi_raw.hpp"
#include "boost/spirit/home/phoenix/bind/bind_function.hpp"

#include "boost/fusion/include/adapt_struct.hpp"
#include "boost/fusion/include/io.hpp"
#include "boost/fusion/include/tuple.hpp"

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

// for attribute filling.
typedef bf::tuple<std::string, std::string> arg_desc_type;
typedef std::vector< arg_desc_type > args_desc;

ostream& operator<<(ostream&os, const arg_desc_type& arg) {
    os << "(" << bf::get<0>(arg)<<","<< bf::get<1>(arg)<<")"<<endl;
    return os;
}

ostream& operator<<(ostream&os, const args_desc& args) {
    for(args_desc::const_iterator it = args.begin(); it != args.end(); ++it)
        os << *it<<endl;
    return os;

}

struct method_reg {
    bool is_ds, is_command, is_object;
    std::string name;
    std::string function;
    std::string completion;
    std::string caller;
    std::string description;
    args_desc args;
};

std::vector<std::string> iface_objects;

ostream& operator<<(ostream& os, const method_reg & m) {
    os << (m.is_ds ? "DS-type" : "Normal");
    os << " name: " << m.name;
    os << " function: " << m.function;
    os << " caller: " << m.caller;
    os << " descr: " << m.description;
    os << " args: " << m.args;
    return os;
}

#if 0
BOOST_FUSION_ADAPT_STRUCT(
    method_reg,
    (bool, is_ds)
    (std::string, name)
    (std::string, function)
    (std::string, caller)
    (std::string, description)
    (args_desc, args)
)
#endif

template <typename Iterator>
struct converter : qi::grammar<Iterator,method_reg(), ascii::space_type>
{

    converter() : converter::base_type(start)
    {
//        methods()->addMethodDS(ptr, method_ds("inState", &StateMachine::inState), "Is the StateMachine in a given state ?", "State", "State Name");
//        to->methods()->addMethod( method("connected",&PortInterface::connected, this),
//                "Check if this port is connected and ready for use.");
        //start = +( *char_ >> addmethod/*[boost::bind(&converter::found_some, this)]*/); // [boost::bind(&converter::parsed_text, this,::_1)];
        addmethod = method_match[boost::bind(&converter::found_some, this)] >> method_name >>','>>method_function >> ',' >> method_caller >> ')'>>',' >> description >> *(','>> arg_name >>',' >> arg_description) >>')' >>';';
        addmethodds = methodds_match[boost::bind(&converter::found_some, this)] >> method_caller >>',' >> "method_ds(" >> method_name >> ','>> method_function >> ')'>>',' >> description >> *(','>> arg_name >>',' >> arg_description) >>')' >>';';

        addcommand = command_match[boost::bind(&converter::found_some, this)] >> command_name >>','>>method_function >> ',' >> completion_function >>','>> method_caller >> -lit(",") >> *(char_ - ',') >>',' >> description >> *(','>> arg_name >>',' >> arg_description) >>')' >>';';
        addobject  = object_match[boost::bind(&converter::found_some, this)] >> object_name >> -( char_(',')>> description >> *(','>> arg_name >>',' >> arg_description)) >>')' >>';';

        start = addmethod[phoenix::ref(method.is_ds) = false] | addmethodds[phoenix::ref(method.is_ds) = true] | addcommand[phoenix::ref(method.is_ds) = false] | addobject[phoenix::ref(method.is_ds) = false];

        method_match = -(lit("methods")>>'('>>')'>>"->")>> -(lit("RTT") >> "::")>> "addMethod" >> '('>>-(lit("RTT") >> "::")>>"method" >>'(';
        methodds_match = -(lit("methods")>>'('>>')'>>"->")>> "addMethodDS" >> '(';
        command_match = -(lit("commands")>>'('>>')'>>"->")>>-(lit("RTT") >> "::")>> "addCommand" >> '('>>-(lit("RTT") >> "::")>>"command" >>'(';
        object_match = ( lit("addCommand") | "addMethod" | "addEvent") >> '(' >> '&';
        object_name = qi::raw[ident][boost::bind(&converter::found_object, this,::_1)];
        method_name = qi::raw[quoted][boost::bind(&converter::found_method, this,::_1)];
        command_name = qi::raw[quoted][boost::bind(&converter::found_command, this,::_1)];
        method_caller = qi::raw[ -char_("&") >> ident][boost::bind(&converter::found_caller, this,::_1)];
        method_function = qi::raw[-char_('&') >> -lit("::") >> *(ident >> "::") >> ident][boost::bind(&converter::found_function, this,::_1)];
        completion_function = qi::raw[-char_('&') >> -lit("::") >> *(ident >> "::") >> ident][boost::bind(&converter::found_completion, this,::_1)];
        description = qi::raw[quoted][boost::bind(&converter::found_desc, this,::_1)];
        arg_name = qi::raw[quoted][boost::bind(&converter::found_arg, this,::_1)];
        arg_description = qi::raw[quoted][boost::bind(&converter::found_arg_desc, this,::_1)];

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
        cout << "Found object name :" << s <<endl;
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
        std::string r = "addOperation( " + m.name +" )";
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
void process_statement(Parser& p, const std::string& regx, const std::string& text, std::string& file1, std::string& file2)
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
             std::cout << "Parsing succeeded\n";
             std::cout << "got: " << p.method << std::endl;
             std::cout << "replace with: " << create_operation(p.method) << endl;
             matches.push_back( bf::tuple<std::string,method_reg>(input,p.method));
        }
        start = what[0].second;
    }
    // loop over both files and replace the matches.
    std::string result = file1;
    bool cont = false;
    do {
        cont = !cont;
        for(std::vector<bf::tuple<std::string,method_reg> >::iterator it = matches.begin(); it!=matches.end(); ++it) {
            //cout << " match '"<< bf::get<0>(*it) <<"' at : " << result.find(bf::get<0>(*it)) <<endl;
            if ( bf::get<1>(*it).is_ds) {
                if ( result.find(bf::get<0>(*it)) != string::npos )
                    result = result.replace( result.find(bf::get<0>(*it)), bf::get<0>(*it).length() , create_operationDS( bf::get<1>(*it) ) );
            } else {
                if ( result.find(bf::get<0>(*it)) != string::npos )
                    result = result.replace( result.find(bf::get<0>(*it)), bf::get<0>(*it).length() , create_operation( bf::get<1>(*it) ) );
            }
        }
        if (cont) {
            file1 = result;
            result = file2;
        } else {
            file2 = result;
        }
    } while (cont);
    //cout << "Final file is:" << endl;
    //cout << result;
}

void process_ports(const std::string& text, std::string& file1, std::string& file2)
{
    bool cont = false;
    std::string::const_iterator start, end;

    // 1: correct addPort instances
    boost::regex e("add(Event)?Port\\(\\s*&\\s*(\\w+)\\s*,\\s*(\"[^\"]*\")\\s*\\)");
    file1 = boost::regex_replace(file1, e,"add\\1Port( \\2 ).doc(\\3)");
    file2 = boost::regex_replace(file2, e,"add\\1Port( \\2 ).doc(\\3)");

    e = boost::regex("add(Event)?Port\\(\\s*&\\s*(\\w+)\\s*\\)");
    file1 = boost::regex_replace(file1, e,"add\\1Port( \\2 )");
    file2 = boost::regex_replace(file2, e,"add\\1Port( \\2 )");

    // 2: Detect ambiguous DataPort and BufferPort:
    e = boost::regex("\\bDataPort(<.*?) (\\w+);");
    vector<string> out_ports, in_ports;

    cont = false;
    boost::smatch what;
    do {
        cont = !cont;
        start = text.begin();
        end = text.end();
        while (boost::regex_search(start, end, what, e))
        {
            unsigned i;
            std::cout << "** Port Match found for "<< e.str() <<" **\n";
            for (i = 1; i < what.size(); ++i) {
                std::cout << "      $" << i << " = \"" << what[i] << "\"\n";
            }
            string type = what[1];
            string name = what[2];
            cout << "Storing OutPort " << name << endl;
            out_ports.push_back( name ); // store port name.
            start = what[0].second;
        }
        if (cont) {
            e = boost::regex("\\bBufferPort(<.*?)(\\w+);");
        }
    } while(cont);

    // 3: Now remove all input ports from out_ports:
    vector<string>::iterator it = out_ports.begin();
    while( it != out_ports.end() ) {
        // if no set or push, it must be an input
        if ( text.find(*it+".Set") == string::npos && text.find(*it+".Push") == string::npos) {
            cout << "Replacing port "<< *it << " as InputPort." <<endl;
            file1 = boost::regex_replace(file1, boost::regex("(\\b)BufferPort(<.*?)"+*it+";"),"\\1InputPort\\2"+*it+";");
            file1 = boost::regex_replace(file1, boost::regex("(\\b)DataPort(<.*?)"+*it+";"),"\\1InputPort\\2"+*it+";");
            file1 = boost::regex_replace(file1, boost::regex(*it+".ready"),*it+".connected");
            file2 = boost::regex_replace(file2, boost::regex("(\\b)BufferPort(<.*?)"+*it+";"),"\\1InputPort\\2"+*it+";");
            file2 = boost::regex_replace(file2, boost::regex("(\\b)DataPort(<.*?)"+*it+";"),"\\1InputPort\\2"+*it+";");
            file2 = boost::regex_replace(file2, boost::regex(*it+".ready"),*it+".connected");
            in_ports.push_back( *it );
            out_ports.erase(it);
            it = out_ports.begin();
        } else {
            cout << "Replacing port "<< *it << " as OutputPort." <<endl;
            file1 = boost::regex_replace(file1, boost::regex("(\\b)BufferPort(<.*?)"+*it+";"),"\\1OutputPort\\2"+*it+";");
            file1 = boost::regex_replace(file1, boost::regex("(\\b)DataPort(<.*?)"+*it+";"),"\\1OutputPort\\2"+*it+";");
            file2 = boost::regex_replace(file2, boost::regex("(\\b)BufferPort(<.*?)"+*it+";"),"\\1OutputPort\\2"+*it+";");
            file2 = boost::regex_replace(file2, boost::regex("(\\b)DataPort(<.*?)"+*it+";"),"\\1OutputPort\\2"+*it+";");
            ++it;
        }
    }

    // 4: Do proper replaces according to our in/out guess:
    for( vector<string>::iterator it = out_ports.begin(); it != out_ports.end(); ++it ) {
        // a Get on an outputport becomes a getLastWrittenValue:
        file1 = boost::regex_replace(file1, boost::regex(*it + ".Get"), *it + ".getLastWrittenValue");
        file2 = boost::regex_replace(file2, boost::regex(*it + ".Get"), *it + ".getLastWrittenValue");
    }

    cont = false;
    string file = file1;
    do {
        cont = !cont;
        // 5: Always right replaces:
        file = boost::regex_replace(file, boost::regex( "(\\w+)\\s*=\\s*(\\w+).Get\\(\\)"), "\\2.read( \\1 )"); // x = p.Get() -> p.read( x )
        file = boost::regex_replace(file, boost::regex( "(\\w+).Get\\(\\s*(\\w+)\\s*\\);"), "\\1.read( \\2 );"); // p.Get( x ); -> p.read( x );
        file = boost::regex_replace(file, boost::regex( "(\\w+).Pop\\(\\s*(\\w+)\\s*\\);"), "\\1.read( \\2 );"); // p.Pop( x ); -> p.read( x );
        file = boost::regex_replace(file, boost::regex( "(\\w+).Pop\\(\\s*(\\w+)\\s*\\)"), "\\1.read( \\2 ) == NewData"); // p.Pop( x ) -> p.read( x ) == NewData
        file = boost::regex_replace(file, boost::regex( "(\\w+).Set\\(\\s*(\\w+)\\s*\\);"), "\\1.write( \\2 );"); // p.Set( x ); -> p.write( x );
        file = boost::regex_replace(file, boost::regex( "(\\w+).Push\\(\\s*(\\w+)\\s*\\)"), "\\1.write( \\2 )"); // p.Push( x ) -> p.write( x )

        // 6: 1-to-1 mappings:
        file = boost::regex_replace(file, boost::regex( "\\bReadDataPort"), "InputPort");
        file = boost::regex_replace(file, boost::regex( "\\bWriteDataPort"), "OutputPort");
        file = boost::regex_replace(file, boost::regex( "\\bReadBufferPort"), "InputPort");
        file = boost::regex_replace(file, boost::regex( "\\bWriteBufferPort"), "OutputPort");
        if (cont) {
            file1 = file;
            file = file2;
        } else {
            file2 = file;
        }
    } while (cont);
}

void process_events(const std::string& text, std::string& file1, std::string& file2)
{
    // 1: Rename event type to operation type.
    boost::regex e("\\bEvent<");
    file1 = boost::regex_replace(file1, e,"Operation<");
    file2 = boost::regex_replace(file2, e,"Operation<");
}

int main(int argc, char** argv) {
    if (argc < 2 || string(argv[1]) == "--help") {
        cerr << "Usage: "<< argv[0] << " component.hpp component.cpp" <<endl;
        cerr << "  At least one cpp or hpp must be given of a component." <<endl;
        return -1;
    }
	//cout << "Opening file..." << endl;

	ifstream file1( argv[1] );
    if ( !file1 ) {
        cout << "Error: " << argv[1] << " does not exist !"<<endl;
        return 1;
    }
    // concatenate both files to form input =  input1 + input2
    string input1, input2, input;
    file1.unsetf( ios_base::skipws );
    istream_iterator<char> streambegin1( file1 );
    istream_iterator<char> streamend1;
    copy( streambegin1, streamend1, back_inserter( input1 ) );
    file1.close();

    if (argc == 3) {
        ifstream file2( argv[2] );
        if ( !file2 ) {
            cout << "Error: " << argv[2] << " does not exist !"<<endl;
            return 1;
        }
        file2.unsetf( ios_base::skipws );
        istream_iterator<char> streambegin2( file2 );
        istream_iterator<char> streamend2;
        copy( streambegin2, streamend2, back_inserter( input2 ) );
        file2.close();
        cout << "Processing "<< argv[1] << " " << argv[2] << endl;
    } else {
        cout << "Processing "<< argv[1] << endl;
    }

    input = input1 + input2;

	typedef std::string::const_iterator iterator_type;
	typedef converter<iterator_type> convert_grammar;

	convert_grammar g;

	std::string parsed = input;
    std::string includes;

    //cout << "input was:" <<endl << input <<endl;
	// First do the method/command to operations conversions:
	process_statement(g,"methods\\(\\).*?;", parsed, input1, input2);
	process_statement(g,"addMethod\\(.*?;", parsed, input1, input2);

    process_statement(g,"commands\\(\\).*?;", parsed, input1, input2);
    process_statement(g,"addCommand\\(.*?;", parsed, input1, input2);

    process_statement(g,"addEvent\\(.*?;", parsed, input1, input2);
    // Do the port conversions:
    process_ports(parsed, input1, input2);

    // Do the remaining event conversions:
    process_events(parsed, input1, input2);

    bool cont = false;
    parsed = input1;
    do {
        cont = !cont;

        // also replace TaskObject and OperationInterface to Service:
        boost::regex tosp("\\bTaskObject\\b");
        boost::regex oisp("\\bOperationInterface\\b");

        parsed = boost::regex_replace(parsed,tosp,"Service");
        parsed = boost::regex_replace(parsed,oisp,"Service");
        parsed = boost::regex_replace(parsed, boost::regex("getObject\\b"),"provides");
        parsed = boost::regex_replace(parsed, boost::regex("\\bCommand.hpp"),"OperationCaller.hpp");
        parsed = boost::regex_replace(parsed, boost::regex("\\bEvent.hpp"),"Operation.hpp");

        parsed = boost::regex_replace(parsed, boost::regex("getMethod(<.*?>)\\("),"getOperation(");
        parsed = boost::regex_replace(parsed, boost::regex("getCommand(<.*?>)\\("),"getOperation(");
        parsed = boost::regex_replace(parsed, boost::regex("getProperty(<.*?>)\\("),"getProperty(");
        parsed = boost::regex_replace(parsed, boost::regex("getAttribute(<.*?>)\\("),"getAttribute(");
        parsed = boost::regex_replace(parsed, boost::regex("getConstant(<.*?>)\\("),"getConstant(");

        parsed = boost::regex_replace(parsed, boost::regex("addObject"),"addService");
        parsed = boost::regex_replace(parsed, boost::regex("removeObject"),"removeService");
        parsed = boost::regex_replace(parsed, boost::regex("internal/Service"),"interface/Service");
        parsed = boost::regex_replace(parsed, boost::regex("internal::Service"),"interface::Service");

        if ( regex_search( parsed, boost::regex("\\bscripting\\(\\)") ) ) {
            cout << "seen scripting" <<endl;
            includes += "#include <rtt/scripting/Scripting.hpp>\n";
        }
        if ( regex_search( parsed, boost::regex("\\marshalling\\(\\)") ) ) {
            includes += "#include <rtt/marsh/Marshalling.hpp>\n";
        }

        parsed = boost::regex_replace(parsed, boost::regex("dynamic_cast<ScriptingAccess*>"),"boost::dynamic_pointer_cast<ScriptingService>");
        parsed = boost::regex_replace(parsed, boost::regex("ScriptingAccess"),"ScriptingService");
        parsed = boost::regex_replace(parsed, boost::regex("ScriptingService*"),"ScriptingService::shared_ptr");
        parsed = boost::regex_replace(parsed, boost::regex("\\bscripting()"),"getProvider<RTT::Scripting>(\"scripting\")");

        parsed = boost::regex_replace(parsed, boost::regex("dynamic_cast<MarshallingAccess*>"),"boost::dynamic_pointer_cast<MarshallingService>");
        parsed = boost::regex_replace(parsed, boost::regex("MarshallingAccess"),"MarshallingService");
        parsed = boost::regex_replace(parsed, boost::regex("MarshallingService*"),"MarshallingService::shared_ptr");
        parsed = boost::regex_replace(parsed, boost::regex("\\bmarshalling()"),"getProvider<RTT::Marshalling>(\"marshalling\")");

        parsed = boost::regex_replace(parsed, boost::regex("\\brecovered\\b"),"recover");

        parsed = boost::regex_replace(parsed, boost::regex("exportPorts\\(\\);"),"");
#ifdef SHORT_NOTATION
        parsed = boost::regex_replace(parsed, boost::regex("->methods\\(\\)"),"");
        parsed = boost::regex_replace(parsed, boost::regex("\\bmethods\\(\\)->"),"");
        parsed = boost::regex_replace(parsed, boost::regex("->commands\\(\\)"),"");
        parsed = boost::regex_replace(parsed, boost::regex("\\bcommands\\(\\)->"),"");
        parsed = boost::regex_replace(parsed, boost::regex("->attributes\\(\\)"),"");
        parsed = boost::regex_replace(parsed, boost::regex("\\battributes\\(\\)->"),"");
        parsed = boost::regex_replace(parsed, boost::regex("\\bproperties\\(\\)->"),"");
        parsed = boost::regex_replace(parsed, boost::regex("\\bevents\\(\\)->"),"");
        parsed = boost::regex_replace(parsed, boost::regex("->events\\(\\)"),"");
#else
        parsed = boost::regex_replace(parsed, boost::regex("->methods\\(\\)"),"->provides()");
        parsed = boost::regex_replace(parsed, boost::regex("\\bmethods\\(\\)->"),"provides()->");
        parsed = boost::regex_replace(parsed, boost::regex("->commands\\(\\)"),"->provides()");
        parsed = boost::regex_replace(parsed, boost::regex("\\bcommands\\(\\)->"),"provides()->");
        parsed = boost::regex_replace(parsed, boost::regex("->attributes\\(\\)"),"->provides()");
        parsed = boost::regex_replace(parsed, boost::regex("\\battributes\\(\\)->"),"provides()->");
        parsed = boost::regex_replace(parsed, boost::regex("\\bproperties\\(\\)->"),"properties()->");
        parsed = boost::regex_replace(parsed, boost::regex("\\bevents\\(\\)->"),"provides()->");
        parsed = boost::regex_replace(parsed, boost::regex("->events\\(\\)"),"->provides()");
#endif
        parsed = boost::regex_replace(parsed, boost::regex("addAttribute\\((\\s*)&(\\w+)"),"addAttribute(\\1\\2");
        parsed = boost::regex_replace(parsed, boost::regex("addConstant\\((\\s*)&(\\w+)"),"addConstant(\\1\\2");
        parsed = boost::regex_replace(parsed, boost::regex("addProperty\\((\\s*)&(\\w+)"),"addProperty(\\1\\2");
        parsed = boost::regex_replace(parsed, boost::regex("\\bEvent<"),"Operation<");
        parsed = boost::regex_replace(parsed, boost::regex("MethodRepository::Factory"),"Service");
        parsed = boost::regex_replace(parsed, boost::regex("BufferPort.hpp"),"Port.hpp");
        parsed = boost::regex_replace(parsed, boost::regex("DataPort.hpp"),"Port.hpp");
        parsed = boost::regex_replace(parsed, boost::regex("ocl/ComponentLoader.hpp"),"ocl/Component.hpp");

        // replace all methods/command objects in an interface with the Operation type:
        for(std::vector<std::string>::iterator it = iface_objects.begin(); it != iface_objects.end(); ++it) {
            parsed = boost::regex_replace(parsed, boost::regex("\\bMethod<(.*?)"+*it),"Operation<\\1"+*it);
            parsed = boost::regex_replace(parsed, boost::regex("\\bCommand<(.*?)"+*it),"Operation<\\1"+*it);
        }
        // replace all command objects for calling to method objects:
        parsed = boost::regex_replace(parsed, boost::regex("\\bCommand<"),"OperationCaller<");

        // finally, add the collected headers:
        parsed = boost::regex_replace(parsed, boost::regex("(.*#include .*?\n)"), "\\1" + includes);
        includes.clear();
        if (cont) {
            ofstream ofile1( argv[1] );
            ofile1 << parsed;
            ofile1.close();
            //cout << parsed;
            if (input2.empty())
                break;
            parsed = input2;
        } else {
            ofstream ofile2( argv[2] );
            ofile2 << parsed;
            ofile2.close();
            //cout << parsed;
        }
    } while (cont);
	return 0;
}

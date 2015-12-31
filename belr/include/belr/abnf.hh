#ifndef abnf_hh
#define abnf_hh


#include "belr.hh"

namespace belr{


/**
 * Grammar containing core definitions of ABNF.
 * This is required for almost all IETF text based protocols.
**/
class CoreRules : public Grammar{
public:
	///Initialize a CoreRules grammar object.
	BELR_PUBLIC CoreRules();
private:
	void alpha();
	void bit();
	void char_();
	void cr();
	void lf();
	void crlf();
	void ctl();
	void digit();
	void dquote();
	void hexdig();
	void htab();
	void lwsp();
	void octet();
	void sp();
	void vchar();
	void wsp();
};



class ABNFGrammar : public Grammar{
public:
	ABNFGrammar();
private:
	void comment();
	void c_nl();
	void c_wsp();
	void rulename();
	void repeat();
	void repeat_min();
	void repeat_max();
	void repeat_count();
	void defined_as();
	void rulelist();
	void rule();
	void elements();
	void alternation();
	void concatenation();
	void repetition();
	void element();
	void group();
	void option();
	void char_val();
	void num_val();
	void prose_val();
	void bin_val();
	void dec_val();
	void hex_val();
	void crlf_or_lf();
};


}//end of namespace
#endif

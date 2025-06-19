/*
 * Copyright (c) 2016-2019 Belledonne Communications SARL.
 *
 * This file is part of belr - a language recognition library for ABNF-defined grammars.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "belr-tester.h"
#include <cstdio>

using namespace ::std;
using namespace ::belr;

typedef struct sip_uri {
	char *user;
	char *host;
	int port;
} sip_uri_t;

typedef struct sip_response {
	sip_uri_t *from;
	sip_uri_t *to;
} sip_response_t;

sip_uri_t *sip_uri_create(void) {
	return bctbx_new0(sip_uri_t, 1);
}

void sip_uri_set_user(sip_uri_t *uri, const char *user) {
	uri->user = bctbx_strdup(user);
}

void sip_uri_set_host(sip_uri_t *uri, const char *host) {
	uri->host = bctbx_strdup(host);
}

void sip_uri_set_port(sip_uri_t *uri, int port) {
	uri->port = port;
}

void sip_uri_destroy(sip_uri_t *uri) {
	if (uri->host) bctbx_free(uri->host);
	if (uri->user) bctbx_free(uri->user);
	bctbx_free(uri);
}

sip_response_t *sip_response_create(void) {
	return bctbx_new0(sip_response_t, 1);
}

void sip_response_set_from(sip_response_t *resp, sip_uri_t *uri) {
	resp->from = uri;
}

void sip_response_set_to(sip_response_t *resp, sip_uri_t *uri) {
	resp->to = uri;
}

void sip_response_destroy(sip_response_t *resp) {
	if (resp->from) sip_uri_destroy(resp->from);
	if (resp->to) sip_uri_destroy(resp->to);
	bctbx_free(resp);
}

static void parser_connected_to_c_functions(void) {
	string grammarToParse = bcTesterRes("sipgrammar.txt");
	string sipmessage = openFile(bcTesterRes("response.txt"));

	BC_ASSERT_TRUE(sipmessage.size() > 0);

	ABNFGrammarBuilder builder;

	// Read grammar put it in object grammar
	shared_ptr<Grammar> grammar = builder.createFromAbnfFile(grammarToParse, make_shared<CoreRules>());

	BC_ASSERT_FALSE(!grammar);

	if (!grammar) return;

	shared_ptr<Parser<void *>> parser = make_shared<Parser<void *>>(grammar);
	parser->setHandler("response", make_fn(&sip_response_create))
	    ->setCollector("from", make_fn(&sip_response_set_from))
	    ->setCollector("to", make_fn(&sip_response_set_to));
	parser->setHandler("from", make_fn(&sip_uri_create))
	    ->setCollector("user", make_fn(&sip_uri_set_user))
	    ->setCollector("host", make_fn(&sip_uri_set_host));
	parser->setHandler("to", make_fn(&sip_uri_create))
	    ->setCollector("user", make_fn(&sip_uri_set_user))
	    ->setCollector("host", make_fn(&sip_uri_set_host))
	    ->setCollector("port", make_fn(&sip_uri_set_port));
	size_t pos = 0;
	void *elem = parser->parseInput("response", sipmessage, &pos);
	BC_ASSERT_PTR_NOT_NULL(elem);
	if (!elem) return;

	BC_ASSERT_EQUAL((int)pos, (int)sipmessage.size(), int, "%i");

	sip_response_t *resp = (sip_response_t *)elem;
	sip_uri_t *from = resp->from;
	sip_uri_t *to = resp->to;
	BC_ASSERT_PTR_NOT_NULL(from);
	BC_ASSERT_PTR_NOT_NULL(to);
	if (from && to) {
		BC_ASSERT_STRING_EQUAL(from->user, "smorlat2");
		BC_ASSERT_STRING_EQUAL(to->user, "smorlat2");
		BC_ASSERT_STRING_EQUAL(from->host, "siptest.linphone.org");
		BC_ASSERT_STRING_EQUAL(to->host, "siptest.linphone.org");
		BC_ASSERT_EQUAL(to->port, 5060, int, "%i");
	}
	sip_response_destroy(resp);
}

//
// Parser with inheritance.
//

/* Base class for all parser elements*/
class Object {
public:
	virtual ~Object() = default;
};

/* Base class for SIP headers */
class SipHeader : public Object {
public:
	SipHeader(const string &headerName) : mHeaderName(headerName) {
	}
	const string &getName() const {
		return mHeaderName;
	}

protected:
	void setHeaderName(const string &headerName) {
		mHeaderName = headerName;
	}

private:
	string mHeaderName;
};

/*
 * The SipHeaderHolder is a kind of container for all possible types of SIP headers.
 * This special type is needed so that it is possible to parse headers in an unitary way (not in a full message).
 * It does not have to be part of the parser API. Keep it internal as an utility.
 */
class SipHeaderHolder : public Object {
public:
	SipHeaderHolder(){};
	void setHeader(const shared_ptr<SipHeader> &header) {
		mHeader = header;
	}
	shared_ptr<SipHeader> getHeader() const {
		return mHeader;
	}

private:
	shared_ptr<SipHeader> mHeader;
};

/*
 * SIP From
 */
class SipFrom : public SipHeader {
public:
	SipFrom() : SipHeader("From"){};
	void setUri(const string &uri) {
		mUri = uri;
	}
	const string &getUri() const {
		return mUri;
	}

private:
	string mUri;
};

/*
 * Extension header
 */
class ExtensionHeader : public SipHeader {
public:
	ExtensionHeader() : SipHeader("generic"){};
	void setName(const string &headerName) {
		SipHeader::setHeaderName(headerName);
	}
	void setValue(const string &headerValue) {
		mHeaderValue = headerValue;
	}
	const string &getValue() const {
		return mHeaderValue;
	}

private:
	string mHeaderValue;
};

/*
 * SIP request. Contains headers.
 */
class SipRequest : public Object {
public:
	SipRequest(){};
	void addHeader(const shared_ptr<SipHeader> &header) {
		mHeaders.push_back(header);
	}
	shared_ptr<SipHeader> getHeader(const string &name) {
		for (auto h : mHeaders) {
			if (strcasecmp(h->getName().c_str(), name.c_str()) == 0) return h;
		}
		return nullptr;
	}

	/* Hide this from the API documentation */
	void addHeaderHolder(const shared_ptr<SipHeaderHolder> &holder) {
		if (holder->getHeader()) addHeader(holder->getHeader());
	}

private:
	list<shared_ptr<SipHeader>> mHeaders;
};

static void parser_with_inheritance(void) {
	string grammarToParse = bcTesterRes("sipgrammar.txt");
	string sipmessage = openFile(bcTesterRes("register.txt"));
	size_t pos = 0;

	BC_ASSERT_TRUE(sipmessage.size() > 0);

	ABNFGrammarBuilder builder;

	// Read grammar put it in object grammar
	shared_ptr<Grammar> grammar = builder.createFromAbnfFile(grammarToParse, make_shared<CoreRules>());

	BC_ASSERT_FALSE(!grammar);

	if (!grammar) return;

	shared_ptr<Parser<shared_ptr<Object>>> parser = make_shared<Parser<shared_ptr<Object>>>(grammar);
	/* The request collects headers in a generic way, thanks to the SipHeaderHolder. */
	parser->setHandler("request", make_fn<SipRequest>())
	    ->setCollector("message-header", make_sfn(&SipRequest::addHeaderHolder));
	parser->setHandler("from", make_fn<SipFrom>())->setCollector("sip-uri", make_sfn(&SipFrom::setUri));
	parser->setHandler("extension-header", make_fn<ExtensionHeader>())
	    ->setCollector("header-name", make_sfn(&ExtensionHeader::setName))
	    ->setCollector("header-value", make_sfn(&ExtensionHeader::setValue));

	/* The message-header rule (representing all sub-rules of SIP headers) is handled by the SipHeaderHolder.
	 * It requires explicit collectors for all sub-rules. */
	parser->setHandler("message-header", make_fn<SipHeaderHolder>())
	    ->setCollector("from", make_sfn(&SipHeaderHolder::setHeader))
	    ->setCollector("extension-header", make_sfn(&SipHeaderHolder::setHeader));

	// Parse the full message
	shared_ptr<Object> elem = parser->parseInput("request", sipmessage, &pos);
	BC_ASSERT_TRUE(elem != nullptr);
	if (!elem) return;

	BC_ASSERT_EQUAL(pos, sipmessage.size(), size_t, "%zu");
	shared_ptr<SipRequest> request = dynamic_pointer_cast<SipRequest>(elem);
	BC_ASSERT_TRUE(request != nullptr);
	if (!request) return;

	BC_ASSERT_TRUE(request->getHeader("from") != nullptr);
	BC_ASSERT_TRUE(request->getHeader("CustomHeader") != nullptr);

	// Parse a single sip header

	elem = parser->parseInput("message-header", "From: <sip:bob@example.net>\r\n", &pos);
	BC_ASSERT_TRUE(elem != nullptr);
	if (!elem) return;
	auto holder = dynamic_pointer_cast<SipHeaderHolder>(elem);
	BC_ASSERT_TRUE(holder != nullptr);
	if (!holder) return;
	BC_ASSERT_TRUE(dynamic_pointer_cast<SipFrom>(holder->getHeader()) != nullptr);
}

static test_t tests[] = {TEST_NO_TAG("Parser connected to C functions", parser_connected_to_c_functions),
                         TEST_NO_TAG("Parser with inheritance", parser_with_inheritance)};

test_suite_t parser_suite = {"Parser", NULL, NULL, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests, 0, 0};

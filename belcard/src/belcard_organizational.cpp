#include "belcard/belcard_organizational.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardTitle> BelCardTitle::create() {
	return BelCardGeneric::create<BelCardTitle>();
}

shared_ptr<BelCardTitle> BelCardTitle::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("TITLE", input, NULL);
	return dynamic_pointer_cast<BelCardTitle>(ret);
}

void BelCardTitle::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("TITLE", make_fn(&BelCardTitle::create))
			->setCollector("group", make_sfn(&BelCardTitle::setGroup))
			->setCollector("any-param", make_sfn(&BelCardTitle::addParam))
			->setCollector("TITLE-value", make_sfn(&BelCardTitle::setValue));
}

BelCardTitle::BelCardTitle() : BelCardProperty() {
	setName("TITLE");
}

shared_ptr<BelCardRole> BelCardRole::create() {
	return BelCardGeneric::create<BelCardRole>();
}

shared_ptr<BelCardRole> BelCardRole::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("ROLE", input, NULL);
	return dynamic_pointer_cast<BelCardRole>(ret);
}

void BelCardRole::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("ROLE", make_fn(&BelCardRole::create))
			->setCollector("group", make_sfn(&BelCardRole::setGroup))
			->setCollector("any-param", make_sfn(&BelCardRole::addParam))
			->setCollector("ROLE-value", make_sfn(&BelCardRole::setValue));
}

BelCardRole::BelCardRole() : BelCardProperty() {
	setName("ROLE");
}

shared_ptr<BelCardLogo> BelCardLogo::create() {
	return BelCardGeneric::create<BelCardLogo>();
}

shared_ptr<BelCardLogo> BelCardLogo::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("LOGO", input, NULL);
	return dynamic_pointer_cast<BelCardLogo>(ret);
}

void BelCardLogo::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("LOGO", make_fn(&BelCardLogo::create))
			->setCollector("group", make_sfn(&BelCardLogo::setGroup))
			->setCollector("any-param", make_sfn(&BelCardLogo::addParam))
			->setCollector("LOGO-value", make_sfn(&BelCardLogo::setValue));
}

BelCardLogo::BelCardLogo() : BelCardProperty() {
	setName("LOGO");
}

shared_ptr<BelCardOrganization> BelCardOrganization::create() {
	return BelCardGeneric::create<BelCardOrganization>();
}

shared_ptr<BelCardOrganization> BelCardOrganization::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("ORG", input, NULL);
	return dynamic_pointer_cast<BelCardOrganization>(ret);
}

void BelCardOrganization::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("ORG", make_fn(&BelCardOrganization::create))
			->setCollector("group", make_sfn(&BelCardOrganization::setGroup))
			->setCollector("any-param", make_sfn(&BelCardOrganization::addParam))
			->setCollector("ORG-value", make_sfn(&BelCardOrganization::setValue));
}

BelCardOrganization::BelCardOrganization() : BelCardProperty() {
	setName("ORG");
}

shared_ptr<BelCardMember> BelCardMember::create() {
	return BelCardGeneric::create<BelCardMember>();
}

shared_ptr<BelCardMember> BelCardMember::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("MEMBER", input, NULL);
	return dynamic_pointer_cast<BelCardMember>(ret);
}

void BelCardMember::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("MEMBER", make_fn(&BelCardMember::create))
			->setCollector("group", make_sfn(&BelCardMember::setGroup))
			->setCollector("any-param", make_sfn(&BelCardMember::addParam))
			->setCollector("MEMBER-value", make_sfn(&BelCardMember::setValue));
}

BelCardMember::BelCardMember() : BelCardProperty() {
	setName("MEMBER");
}

shared_ptr<BelCardRelated> BelCardRelated::create() {
	return BelCardGeneric::create<BelCardRelated>();
}

shared_ptr<BelCardRelated> BelCardRelated::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("RELATED", input, NULL);
	return dynamic_pointer_cast<BelCardRelated>(ret);
}

void BelCardRelated::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("RELATED", make_fn(&BelCardRelated::create))
			->setCollector("group", make_sfn(&BelCardRelated::setGroup))
			->setCollector("any-param", make_sfn(&BelCardRelated::addParam))
			->setCollector("RELATED-value", make_sfn(&BelCardRelated::setValue));
}

BelCardRelated::BelCardRelated() : BelCardProperty() {
	setName("RELATED");
}
/*
 * is-composing.cpp
 * Copyright (C) 2010-2017 Belledonne Communications SARL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "linphone/utils/utils.h"

#include "chat/chat-room/chat-room-p.h"
#include "logger/logger.h"

#include "chat/is-composing.h"

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

const string IsComposing::isComposingPrefix = "/xsi:isComposing";

// -----------------------------------------------------------------------------

IsComposing::IsComposing (LinphoneCore *core, IsComposingListener *listener)
	: core(core), listener(listener) {}

IsComposing::~IsComposing () {
	stopTimers();
}

// -----------------------------------------------------------------------------

string IsComposing::marshal (bool isComposing) {
	string content;

	xmlBufferPtr buf = xmlBufferCreate();
	if (!buf) {
		lError() << "Error creating the XML buffer";
		return content;
	}
	xmlTextWriterPtr writer = xmlNewTextWriterMemory(buf, 0);
	if (!writer) {
		lError() << "Error creating the XML writer";
		return content;
	}

	int err = xmlTextWriterStartDocument(writer, "1.0", "UTF-8", nullptr);
	if (err >= 0) {
		err = xmlTextWriterStartElementNS(writer, nullptr, (const xmlChar *)"isComposing",
			(const xmlChar *)"urn:ietf:params:xml:ns:im-iscomposing");
	}
	if (err >= 0) {
		err = xmlTextWriterWriteAttributeNS(writer, (const xmlChar *)"xmlns", (const xmlChar *)"xsi", nullptr,
			(const xmlChar *)"http://www.w3.org/2001/XMLSchema-instance");
	}
	if (err >= 0) {
		err = xmlTextWriterWriteAttributeNS(writer, (const xmlChar *)"xsi", (const xmlChar *)"schemaLocation", nullptr,
			(const xmlChar *)"urn:ietf:params:xml:ns:im-composing iscomposing.xsd");
	}
	if (err >= 0) {
		err = xmlTextWriterWriteElement(writer, (const xmlChar *)"state",
			isComposing ? (const xmlChar *)"active" : (const xmlChar *)"idle");
	}
	if ((err >= 0) && isComposing) {
		int refreshTimeout = lp_config_get_int(core->config, "sip", "composing_refresh_timeout", defaultRefreshTimeout);
		err = xmlTextWriterWriteElement(writer, (const xmlChar *)"refresh", (const xmlChar *)Utils::toString(refreshTimeout).c_str());
	}
	if (err >= 0) {
		/* Close the "isComposing" element. */
		err = xmlTextWriterEndElement(writer);
	}
	if (err >= 0) {
		err = xmlTextWriterEndDocument(writer);
	}
	if (err > 0) {
		/* xmlTextWriterEndDocument returns the size of the content. */
		content = (char *)buf->content;
	}
	xmlFreeTextWriter(writer);
	xmlBufferFree(buf);
	return content;
}

void IsComposing::parse (const string &text) {
	xmlparsing_context_t *xmlCtx = linphone_xmlparsing_context_new();
	xmlSetGenericErrorFunc(xmlCtx, linphone_xmlparsing_genericxml_error);
	xmlCtx->doc = xmlReadDoc((const unsigned char *)text.c_str(), 0, nullptr, 0);
	if (xmlCtx->doc)
		parse(xmlCtx);
	else
		lWarning() << "Wrongly formatted presence XML: " << xmlCtx->errorBuffer;
	linphone_xmlparsing_context_destroy(xmlCtx);
}

void IsComposing::startIdleTimer () {
	unsigned int duration = getIdleTimerDuration();
	if (!idleTimer) {
		idleTimer = core->sal->create_timer(idleTimerExpired, this,
			duration * 1000, "composing idle timeout");
	} else {
		belle_sip_source_set_timeout(idleTimer, duration * 1000);
	}
}

void IsComposing::startRefreshTimer () {
	unsigned int duration = getRefreshTimerDuration();
	if (!refreshTimer) {
		refreshTimer = core->sal->create_timer(refreshTimerExpired, this,
			duration * 1000, "composing refresh timeout");
	} else {
		belle_sip_source_set_timeout(refreshTimer, duration * 1000);
	}
}

void IsComposing::startRemoteRefreshTimer (const char *refreshStr) {
	unsigned int duration = getRemoteRefreshTimerDuration();
	if (refreshStr)
		duration = static_cast<unsigned int>(Utils::stoi(refreshStr));
	if (!remoteRefreshTimer) {
		remoteRefreshTimer = core->sal->create_timer(remoteRefreshTimerExpired, this,
			duration * 1000, "composing remote refresh timeout");
	} else {
		belle_sip_source_set_timeout(remoteRefreshTimer, duration * 1000);
	}
}

#if 0
void IsComposing::idleTimerExpired () {
	stopRefreshTimer();
	stopIdleTimer();
}
#endif

void IsComposing::stopTimers () {
	stopIdleTimer();
	stopRefreshTimer();
	stopRemoteRefreshTimer();
}

// -----------------------------------------------------------------------------

void IsComposing::stopIdleTimer () {
	if (idleTimer) {
		if (core && core->sal)
			core->sal->cancel_timer(idleTimer);
		belle_sip_object_unref(idleTimer);
		idleTimer = nullptr;
	}
}

void IsComposing::stopRefreshTimer () {
	if (refreshTimer) {
		if (core && core->sal)
			core->sal->cancel_timer(refreshTimer);
		belle_sip_object_unref(refreshTimer);
		refreshTimer = nullptr;
	}
}

void IsComposing::stopRemoteRefreshTimer () {
	if (remoteRefreshTimer) {
		if (core && core->sal)
			core->sal->cancel_timer(remoteRefreshTimer);
		belle_sip_object_unref(remoteRefreshTimer);
		remoteRefreshTimer = nullptr;
	}
}

// -----------------------------------------------------------------------------

unsigned int IsComposing::getIdleTimerDuration () {
	int idleTimerDuration = lp_config_get_int(core->config, "sip", "composing_idle_timeout", defaultIdleTimeout);
	return idleTimerDuration < 0 ? 0 : static_cast<unsigned int>(idleTimerDuration);
}

unsigned int IsComposing::getRefreshTimerDuration () {
	int refreshTimerDuration = lp_config_get_int(core->config, "sip", "composing_refresh_timeout", defaultRefreshTimeout);
	return refreshTimerDuration < 0 ? 0 : static_cast<unsigned int>(refreshTimerDuration);
}

unsigned int IsComposing::getRemoteRefreshTimerDuration () {
	int remoteRefreshTimerDuration = lp_config_get_int(core->config, "sip", "composing_remote_refresh_timeout", defaultRemoteRefreshTimeout);
	return remoteRefreshTimerDuration < 0 ? 0 : static_cast<unsigned int>(remoteRefreshTimerDuration);
}

void IsComposing::parse (xmlparsing_context_t *xmlCtx) {
	char xpathStr[MAX_XPATH_LENGTH];
	char *stateStr = nullptr;
	char *refreshStr = nullptr;
	int i;
	bool state = false;

	if (linphone_create_xml_xpath_context(xmlCtx) < 0)
		return;

	xmlXPathRegisterNs(xmlCtx->xpath_ctx, (const xmlChar *)"xsi", (const xmlChar *)"urn:ietf:params:xml:ns:im-iscomposing");
	xmlXPathObjectPtr isComposingObject = linphone_get_xml_xpath_object_for_node_list(xmlCtx, isComposingPrefix.c_str());
	if (isComposingObject) {
		if (isComposingObject->nodesetval) {
			for (i = 1; i <= isComposingObject->nodesetval->nodeNr; i++) {
				snprintf(xpathStr, sizeof(xpathStr), "%s[%i]/xsi:state", isComposingPrefix.c_str(), i);
				stateStr = linphone_get_xml_text_content(xmlCtx, xpathStr);
				if (!stateStr)
					continue;
				snprintf(xpathStr, sizeof(xpathStr), "%s[%i]/xsi:refresh", isComposingPrefix.c_str(), i);
				refreshStr = linphone_get_xml_text_content(xmlCtx, xpathStr);
			}
		}
		xmlXPathFreeObject(isComposingObject);
	}

	if (stateStr) {
		if (strcmp(stateStr, "active") == 0) {
			state = true;
			startRemoteRefreshTimer(refreshStr);
		} else {
			stopRemoteRefreshTimer();
		}

		listener->onIsRemoteComposingStateChanged(state);
		linphone_free_xml_text_content(stateStr);
	}
	if (refreshStr)
		linphone_free_xml_text_content(refreshStr);
}

int IsComposing::idleTimerExpired (unsigned int revents) {
	listener->onIsComposingStateChanged(false);
	return BELLE_SIP_STOP;
}

int IsComposing::refreshTimerExpired (unsigned int revents) {
	listener->onIsComposingRefreshNeeded();
	return BELLE_SIP_CONTINUE;
}

int IsComposing::remoteRefreshTimerExpired (unsigned int revents) {
	stopRemoteRefreshTimer();
	listener->onIsRemoteComposingStateChanged(false);
	return BELLE_SIP_STOP;
}

int IsComposing::idleTimerExpired (void *data, unsigned int revents) {
	IsComposing *d = reinterpret_cast<IsComposing *>(data);
	return d->idleTimerExpired(revents);
}

int IsComposing::refreshTimerExpired (void *data, unsigned int revents) {
	IsComposing *d = reinterpret_cast<IsComposing *>(data);
	return d->refreshTimerExpired(revents);
}

int IsComposing::remoteRefreshTimerExpired (void *data, unsigned int revents) {
	IsComposing *d = reinterpret_cast<IsComposing *>(data);
	return d->remoteRefreshTimerExpired(revents);
}

LINPHONE_END_NAMESPACE

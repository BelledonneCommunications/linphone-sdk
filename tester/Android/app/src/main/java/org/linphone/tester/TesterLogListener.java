package org.linphone.tester;

public interface TesterLogListener {
    void onMessage(String message);
    void onError(String message);
}

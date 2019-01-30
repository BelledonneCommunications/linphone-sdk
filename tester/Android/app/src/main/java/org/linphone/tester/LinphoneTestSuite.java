package org.linphone.tester;

/*
 LinphoneTestSuite.java
 Copyright (C) 2018  Belledonne Communications, Grenoble, France

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import java.util.ArrayList;
import java.util.List;

public class LinphoneTestSuite implements TesterLogListener {
    private List<String> mList;

    protected LinphoneTestSuite() {
        mList = new ArrayList<>();
        if (!Tester.getInstance().isReady()) {
            Tester.getInstance().installTester();
        }
        Tester.getInstance().setListener(this);
    }

    public List<String> getList() {
        return mList;
    }

    public void run(String[] args) {
        Tester.getInstance().run(args);
    }

    @Override
    public void onMessage(String message) {
        if (message != null && message.length() > 0) {
            mList.add(message);
        }
    }

    @Override
    public void onError(String message) {

    }
}

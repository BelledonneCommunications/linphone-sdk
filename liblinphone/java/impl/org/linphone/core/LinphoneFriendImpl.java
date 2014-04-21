/*
LinphoneFriendImpl.java
Copyright (C) 2010  Belledonne Communications, Grenoble, France

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
package org.linphone.core;

import java.io.Serializable;

class LinphoneFriendImpl implements LinphoneFriend, Serializable {
	protected final long nativePtr;
	private native long newLinphoneFriend(String friendUri);
	private native void setAddress(long nativePtr,long friend);
	private native long getAddress(long nativePtr);
	private native void setIncSubscribePolicy(long nativePtr,int enumValue);
	private native int  getIncSubscribePolicy(long nativePtr);
	private native void enableSubscribes(long nativePtr,boolean value);
	private native boolean isSubscribesEnabled(long nativePtr);
	private native int getStatus(long nativePtr);
	private native Object getPresenceModel(long nativePtr);
	private native void setPresenceModel(long nativePtr, long presencePtr);
	private native void edit(long nativePtr);
	private native void done(long nativePtr);
	private native void  delete(long ptr);
	private native Object getCore(long ptr);
	
	boolean ownPtr = false;
	protected LinphoneFriendImpl()  {
		nativePtr = newLinphoneFriend(null);
	}	
	protected LinphoneFriendImpl(String friendUri)  {
		nativePtr = newLinphoneFriend(friendUri);
	}
	protected LinphoneFriendImpl(long aNativePtr)  {
		nativePtr = aNativePtr;
		ownPtr=false;
	}
	protected void finalize() throws Throwable {
		if (ownPtr) delete(nativePtr);
	}
	public void setAddress(LinphoneAddress anAddress) {
		this.setAddress(nativePtr, ((LinphoneAddressImpl)anAddress).nativePtr);
	}
	public LinphoneAddress getAddress() {
		return new LinphoneAddressImpl(getAddress(nativePtr),LinphoneAddressImpl.WrapMode.FromConst);
	}
	public void setIncSubscribePolicy(SubscribePolicy policy) {
		synchronized(getSyncObject()){
			setIncSubscribePolicy(nativePtr,policy.mValue);
		}
	}
	public SubscribePolicy getIncSubscribePolicy() {
		return SubscribePolicy.fromInt(getIncSubscribePolicy(nativePtr)) ;
	}
	public void enableSubscribes(boolean enable) {
		synchronized(getSyncObject()){
			enableSubscribes(nativePtr, enable);
		}
	}
	public boolean isSubscribesEnabled() {
		return isSubscribesEnabled(nativePtr);
	}
	public OnlineStatus getStatus() {
		return OnlineStatus.fromInt(getStatus(nativePtr));
	}
	public PresenceModel getPresenceModel() {
		return (PresenceModel)getPresenceModel(nativePtr);
	}
	public void edit() {
		synchronized(getSyncObject()){
			edit(nativePtr);
		}
	}
	public void done() {
		synchronized(getSyncObject()){
			done(nativePtr);
		}
	}
	public long getNativePtr() {
		return nativePtr;
	}
	
	/*
	 * Returns a java object to synchronize this friend with.
	 * Indeed some operation must be synchronized with the LinphoneCore object.
	 * If the friend is not associated with a LinphoneCore object, it returns itself in order to avoid writing code for case where no synchronization is necessary.
	 */
	private Object getSyncObject(){
		Object core=getCore(nativePtr);
		if (core!=null) return core;
		else return this;
	}
}

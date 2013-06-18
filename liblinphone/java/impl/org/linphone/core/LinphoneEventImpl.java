package org.linphone.core;

public class LinphoneEventImpl implements LinphoneEvent {
	private Object mUserContext;
	private long mNativePtr;
	
	protected LinphoneEventImpl(long nativePtr){
		mNativePtr=nativePtr;
	}
	
	private native String getEventName(long nativeptr);
	@Override
	public String getEventName() {
		return getEventName(mNativePtr);
	}

	private native int acceptSubscription(long nativeptr);
	@Override
	public void acceptSubscription() {
		acceptSubscription(mNativePtr);
	}

	private native int denySubscription(long nativeptr, int reason);
	@Override
	public void denySubscription(Reason reason) {
		denySubscription(mNativePtr,reason.mValue);
	}

	private native int notify(long nativeptr, String type, String subtype, String data);
	@Override
	public void notify(LinphoneContent content) {
		notify(mNativePtr,content.getType(),content.getSubtype(),content.getDataAsString());
	}

	private native int updateSubscribe(long nativePtr, String type, String subtype, String data);
	@Override
	public void updateSubscribe(LinphoneContent content) {
		updateSubscribe(mNativePtr,content.getType(), content.getSubtype(),content.getDataAsString());
	}

	private native int updatePublish(long nativePtr, String type, String subtype, String data);
	@Override
	public void updatePublish(LinphoneContent content) {
		updatePublish(mNativePtr,content.getType(), content.getSubtype(),content.getDataAsString());
	}

	private native int terminate(long nativePtr);
	@Override
	public void terminate() {
		terminate(mNativePtr);
	}

	private native int getReason(long nativePtr);
	@Override
	public Reason getReason() {
		return Reason.fromInt(getReason(mNativePtr));
	}

	@Override
	public void setUserContext(Object obj) {
		mUserContext=obj;
	}

	@Override
	public Object getUserContext() {
		return mUserContext;
	}

	private native int getSubscriptionDir(long nativeptr);
	@Override
	public SubscriptionDir getSubscriptionDir() {
		return SubscriptionDir.fromInt(getSubscriptionDir(mNativePtr));
	}

	private native int getSubscriptionState(long nativeptr);
	@Override
	public SubscriptionState getSubscriptionState() {
		try {
			return SubscriptionState.fromInt(getSubscriptionState(mNativePtr));
		} catch (LinphoneCoreException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return SubscriptionState.Error;
	}
	private native void unref(long nativeptr);
	protected void finalize(){
		unref(mNativePtr);
	}

}

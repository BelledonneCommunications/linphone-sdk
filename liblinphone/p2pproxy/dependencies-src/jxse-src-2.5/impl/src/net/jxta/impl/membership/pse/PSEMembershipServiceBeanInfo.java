/*
 * Copyright (c) 2001-2007 Sun Microsystems, Inc.  All rights reserved.
 *  
 *  The Sun Project JXTA(TM) Software License
 *  
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions are met:
 *  
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  
 *  2. Redistributions in binary form must reproduce the above copyright notice, 
 *     this list of conditions and the following disclaimer in the documentation 
 *     and/or other materials provided with the distribution.
 *  
 *  3. The end-user documentation included with the redistribution, if any, must 
 *     include the following acknowledgment: "This product includes software 
 *     developed by Sun Microsystems, Inc. for JXTA(TM) technology." 
 *     Alternately, this acknowledgment may appear in the software itself, if 
 *     and wherever such third-party acknowledgments normally appear.
 *  
 *  4. The names "Sun", "Sun Microsystems, Inc.", "JXTA" and "Project JXTA" must 
 *     not be used to endorse or promote products derived from this software 
 *     without prior written permission. For written permission, please contact 
 *     Project JXTA at http://www.jxta.org.
 *  
 *  5. Products derived from this software may not be called "JXTA", nor may 
 *     "JXTA" appear in their name, without prior written permission of Sun.
 *  
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED WARRANTIES,
 *  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
 *  FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SUN 
 *  MICROSYSTEMS OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
 *  OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  
 *  JXTA is a registered trademark of Sun Microsystems, Inc. in the United 
 *  States and other countries.
 *  
 *  Please see the license information page at :
 *  <http://www.jxta.org/project/www/license.html> for instructions on use of 
 *  the license in source files.
 *  
 *  ====================================================================
 *  
 *  This software consists of voluntary contributions made by many individuals 
 *  on behalf of Project JXTA. For more information on Project JXTA, please see 
 *  http://www.jxta.org.
 *  
 *  This license is based on the BSD license adopted by the Apache Foundation. 
 */
package net.jxta.impl.membership.pse;


import java.beans.BeanDescriptor;
import java.beans.EventSetDescriptor;
import java.beans.SimpleBeanInfo;
import java.beans.PropertyDescriptor;

import java.beans.IntrospectionException;
import java.lang.reflect.UndeclaredThrowableException;


/**
 *  Our beaninfo
 **/
public class PSEMembershipServiceBeanInfo extends SimpleBeanInfo {
    
    /**
     *  {@inheritDoc}
     **/
    @Override
    public BeanDescriptor getBeanDescriptor() {
        return new BeanDescriptor(PSEMembershipService.class);
    }
    
    /**
     *  {@inheritDoc}
     **/
    @Override
    public EventSetDescriptor[] getEventSetDescriptors() {
        try {
            EventSetDescriptor changed = new EventSetDescriptor(PSEMembershipService.class, "propertyChange"
                    ,
                    java.beans.PropertyChangeListener.class, "propertyChange");
            
            changed.setDisplayName("bound property change");
            
            EventSetDescriptor[] rv = { changed };

            return rv;
        } catch (IntrospectionException failed) {
            throw new UndeclaredThrowableException(failed, "Configuration error");
        }
    }
    
    /**
     *  {@inheritDoc}
     **/
    @Override
    public PropertyDescriptor[] getPropertyDescriptors() {
        try {
            PropertyDescriptor defaultcredential = new PropertyDescriptor("defaultCredential", PSEMembershipService.class
                    ,
                    "getDefaultCredential", "setDefaultCredential");

            defaultcredential.setBound(true);
            
            PropertyDescriptor addcredential = new PropertyDescriptor("addCredential", PSEMembershipService.class);

            addcredential.setBound(true);
            
            PropertyDescriptor rv[] = { addcredential, defaultcredential };
            
            return rv;
        } catch (IntrospectionException failed) {
            throw new UndeclaredThrowableException(failed, "Configuration error");
        }
    }
}


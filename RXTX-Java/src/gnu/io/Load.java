/*-------------------------------------------------------------------------
|   RXTX License v 2.1 - LGPL v 2.1 + Linking Over Controlled Interface.
|   RXTX is a native interface to serial ports in java.
|   Copyright 1997-2007 by Trent Jarvi tjarvi@qbang.org and others who
|   actually wrote it.  See individual source files for more information.
|
|   A copy of the LGPL v 2.1 may be found at
|   http://www.gnu.org/licenses/lgpl.txt on March 4th 2007.  A copy is
|   here for your convenience.
|
|   This library is free software; you can redistribute it and/or
|   modify it under the terms of the GNU Lesser General Public
|   License as published by the Free Software Foundation; either
|   version 2.1 of the License, or (at your option) any later version.
|
|   This library is distributed in the hope that it will be useful,
|   but WITHOUT ANY WARRANTY; without even the implied warranty of
|   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
|   Lesser General Public License for more details.
|
|   An executable that contains no derivative of any portion of RXTX, but
|   is designed to work with RXTX by being dynamically linked with it,
|   is considered a "work that uses the Library" subject to the terms and
|   conditions of the GNU Lesser General Public License.
|
|   The following has been added to the RXTX License to remove
|   any confusion about linking to RXTX.   We want to allow in part what
|   section 5, paragraph 2 of the LGPL does not permit in the special
|   case of linking over a controlled interface.  The intent is to add a
|   Java Specification Request or standards body defined interface in the 
|   future as another exception but one is not currently available.
|
|   http://www.fsf.org/licenses/gpl-faq.html#LinkingOverControlledInterface
|
|   As a special exception, the copyright holders of RXTX give you
|   permission to link RXTX with independent modules that communicate with
|   RXTX solely through the Sun Microsytems CommAPI interface version 2,
|   regardless of the license terms of these independent modules, and to copy
|   and distribute the resulting combined work under terms of your choice,
|   provided that every copy of the combined work is accompanied by a complete
|   copy of the source code of RXTX (the version of RXTX used to produce the
|   combined work), being distributed under the terms of the GNU Lesser General
|   Public License plus this exception.  An independent module is a
|   module which is not derived from or based on RXTX.
|
|   Note that people who make modified versions of RXTX are not obligated
|   to grant this special exception for their modified versions; it is
|   their choice whether to do so.  The GNU Lesser General Public License
|   gives permission to release a modified version without this exception; this
|   exception also makes it possible to release a modified version which
|   carries forward this exception.
|
|   You should have received a copy of the GNU Lesser General Public
|   License along with this library; if not, write to the Free
|   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
|   All trademarks belong to their respective owners.
--------------------------------------------------------------------------*/


package gnu.io;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Map;
import java.util.Properties;

public final class Load {

    public static void library(String library) {

        //check if the code runs in an applet
        if ("true".equals(System.getProperty("java.version.applet"))){
            
            //applet detected
            loadLibrary(library);
        } else {
            System.loadLibrary(library);
        }
    }

    public static void rxtx() {
        if (System.getProperty("os.arch").equals("x86")) {
            library("rxtxSerial");
        } else {
            library("rxtxSerial64");
        }
    }

    public static void loadLibrary(final String libraryName) {
        AccessController.doPrivileged(new PrivilegedAction() {

            public Object run() {
                loadLibraryInternal(libraryName);
                return new Object();
            }
        });
    }

    private static void loadLibraryInternal(String libraryName) {
        String sunAppletLauncher = System.getProperty("sun.jnlp.applet.launcher");
        boolean usingJNLPAppletLauncher = Boolean.valueOf(sunAppletLauncher).booleanValue();
        boolean loaded = false;
        if (usingJNLPAppletLauncher) {
            try {
                Class jnlpAppletLauncherClass = Class.forName("org.jdesktop.applet.util.JNLPAppletLauncher");
                Method jnlpLoadLibraryMethod = jnlpAppletLauncherClass.getDeclaredMethod("loadLibrary", new Class[]{String.class});
                jnlpLoadLibraryMethod.invoke(null, new Object[]{libraryName});
                loaded = true;
            } catch (ExceptionInInitializerError ex) {
                System.err.println("org.jdesktop.applet.util.JNLPAppletLauncher static initialization exploded!");
                ex.printStackTrace();
            } catch (ClassNotFoundException ex) {
                System.err.println("org.jdesktop.applet.util.JNLPAppletLauncher not found, attempting to use System.loadLibrary instead.");
            } catch (Exception ex) {
                Throwable t = ex;
                if (t instanceof InvocationTargetException) {
                    t = ((InvocationTargetException) t).getTargetException();
                }
                if (t instanceof Error) {
                    throw (Error) t;
                }
                if (t instanceof RuntimeException) {
                    throw (RuntimeException) t;
                }
                throw (UnsatisfiedLinkError) (new UnsatisfiedLinkError()).initCause(ex);
            }
        }
        if (!loaded) {
            System.loadLibrary(libraryName);
        }
    }

    public static void printSystemProperties() {
        AccessController.doPrivileged(new PrivilegedAction() {

            public Object run() {
                Properties properties = System.getProperties();
                System.out.println("System properties:");
                for (Map.Entry<Object, Object> entry : properties.entrySet()) {

                    System.out.println(" " + entry.getKey() + " : " + entry.getValue());

                }
                return new Object();
            }
        });

    }
}

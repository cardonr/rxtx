/*-------------------------------------------------------------------------
|   RXTX License v 2.1 - LGPL v 2.1 + Linking Over Controlled Interface.
|   RXTX is a native interface to serial ports in java.
|   Copyright 1997-2008 by Trent Jarvi tjarvi@qbang.org and others who
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
package gnu.io.testrxtx;

import gnu.io.RXTXPort;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/**
 *
 * @author Rodolphe
 */
public class Tx implements Runnable {

    RXTXPort rxtxPort;

    /**
     *
     * Constructor
     */
    public Tx() throws Exception {

    }

    public void run() {
        try {

            //wait 1 sec to be sure that the receiver is ready.
            Thread.sleep(200);

            rxtxPort = new RXTXPort("COM1");
            rxtxPort.setSerialPortParams(2400, RXTXPort.DATABITS_8, RXTXPort.STOPBITS_1, RXTXPort.PARITY_NONE);
            rxtxPort.setFlowControlMode(RXTXPort.FLOWCONTROL_RTSCTS_IN + RXTXPort.FLOWCONTROL_RTSCTS_OUT);

            OutputStream os = rxtxPort.getOutputStream();
            os.write(TestRxTx.MESSAGE1.getBytes("UTF-8"));
            os.flush();

            Thread.sleep(2000);

            System.out.println("Tx sent message 1");

            byte[] buffer = new byte[64];
            ByteArrayOutputStream bos = new ByteArrayOutputStream(64);
            InputStream is = rxtxPort.getInputStream();
            int numOfBytes = 0;
            while (numOfBytes < TestRxTx.MESSAGE2.length()) {

                if (is.available() > 0) {
                    int read = is.read(buffer, 0, buffer.length);
                    if (read < 0) {
                        //end of stream
                        break;
                    } else {
                        bos.write(buffer, 0, read);
                        numOfBytes += read;
                        System.out.println("Tx current numOfBytes:" + numOfBytes);
                    }

                } else {
                    Thread.sleep(100);
                }
            }
            bos.close();
            String receivedStr = new String(bos.toByteArray(), "UTF-8");
            if (receivedStr.equals(TestRxTx.MESSAGE2)) {
                //OK
                System.out.println("Tx received message 2");
            } else {
                throw new Exception("bad string received: " + receivedStr);
            }

            os.close();
            System.out.println("Tx:os closed");

            is.close();
            System.out.println("Tx:is closed");

            rxtxPort.close();
            System.out.println("Tx:RxTxPort closed");

        } catch (Exception e) {
            System.err.println("Tx error");
            e.printStackTrace();
        }

    }

}

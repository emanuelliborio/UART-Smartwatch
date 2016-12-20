/*
 * The MIT License (MIT)

Copyright (c) 2016 Jochen Peters (JotPe, Krefeld)

Permission is hereby granted, free of charge, to any person obtaining 
a copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.
*/
package click.dummer.UartSmartwatch;

import android.app.Notification;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;

public class NotificationService extends NotificationListenerService {
    private SharedPreferences mPreferences;
    private String lastPost = "";

    @Override
    public void onNotificationPosted(StatusBarNotification sbn) {
        Notification noti = sbn.getNotification();
        Bundle extras = noti.extras;
        String title = extras.getString(Notification.EXTRA_TITLE);
        String msg = (String) noti.tickerText;

        if (!sbn.isClearable()) return;

        if (title.equals(lastPost) && msg == null) {
            return;
        } else {
            lastPost = title;
        }

        if (msg == null) return;

        mPreferences = PreferenceManager.getDefaultSharedPreferences(getApplicationContext());
        int messageLimit = Integer.parseInt(mPreferences.getString("messageLimit", "100"));

        Intent i = new  Intent("click.dummer.UartNotify.NOTIFICATION_LISTENER");
        if (msg.length() > messageLimit) {
            msg = title;
        }
        i.putExtra("MSG", msg);
        i.putExtra("posted", true);
        sendBroadcast(i);
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn) {
        Intent i = new  Intent("click.dummer.UartNotify.NOTIFICATION_LISTENER");
        i.putExtra("MSG", "notify removed");
        i.putExtra("posted", false);
        sendBroadcast(i);
    }
}

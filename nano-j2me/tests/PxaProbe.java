/* SPDX-License-Identifier: GPL-3.0-or-later */
import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.rms.RecordStore;
import javax.microedition.media.Manager;

/** An original fixture: real Java bytecode, input, graphics and durable RMS. */
public class PxaProbe extends MIDlet {
    protected void startApp() {
        try {
            RecordStore store = RecordStore.openRecordStore("probe", true);
            int old = store.getNumRecords() == 0 ? 0 : store.getRecord(1)[0] & 255;
            byte[] value = new byte[] {(byte)(old + 1)};
            if (store.getNumRecords() == 0) store.addRecord(value, 0, 1);
            else store.setRecord(1, value, 0, 1);
            store.closeRecordStore();
            System.out.println("PXA_PROBE RMS_OLD=" + old);
            Manager.playTone(69, 100, 30);
        } catch (Exception e) {
            System.out.println("PXA_PROBE ERROR=" + e.toString());
        }
        Display.getDisplay(this).setCurrent(new ProbeCanvas());
        System.out.println("PXA_PROBE READY");
    }
    protected void pauseApp() {}
    protected void destroyApp(boolean unconditional) { System.out.println("PXA_PROBE DESTROY"); }
    static class ProbeCanvas extends Canvas {
        private int color = 0x2080c0;
        private boolean loggedSize;
        protected void paint(Graphics g) {
            if (!loggedSize) { System.out.println("PXA_PROBE CANVAS=" + getWidth() + "x" + getHeight()); loggedSize = true; }
            g.setColor(color); g.fillRect(0, 0, getWidth(), getHeight());
            g.setColor(0xffffff); g.drawString("PXA J2ME", 12, 20, Graphics.TOP | Graphics.LEFT);
            g.drawString("中文字体 / PXA", 12, 44, Graphics.TOP | Graphics.LEFT);
            g.setColor(0x40d060);g.fillRect(20, 80, 80, 80);
        }
        protected void keyPressed(int code) {
            System.out.println("PXA_PROBE KEY_DOWN=" + code); color = 0x902040; repaint();
        }
        protected void keyReleased(int code) { System.out.println("PXA_PROBE KEY_UP=" + code); }
        protected void pointerPressed(int x, int y) { System.out.println("PXA_PROBE POINTER_DOWN=" + x + "," + y); }
        protected void pointerReleased(int x, int y) { System.out.println("PXA_PROBE POINTER_UP=" + x + "," + y); }
    }
}

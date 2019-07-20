# scheme-web-view
**A web view for scheme** 

Composed from Windows 10 Web View 2 (win32) and Chez Scheme.

-------

Selfiead

![Selfie](assets/Selfie.png)



*This requires a very up to date Windows 10 version.*

**Reason**

I prefer a workspace type interface rather than a terminal interface.

I like panes rather than overlapping windows; you can adjust these panes a little; there is an invisible splitter between them; you can drag that.

Type in your scheme code; press control-return; the result is displayed below.

Pressing shift-return will run a selected bit of code; I use that a lot.

If you write a function that uses display; that output goes into the wider transcript pane.

Although the text tiles are editable (thanks to code mirror) this is not an editor; visual studio code is a great modern editor for editing scheme and C++ code.

As well as executing scheme functions in the browser based view; you can also have the scheme functions create and execute any JavaScript in the web view; so that makes for reasonably unlimited potential.

The web view provides a communications channel; so that the browser and scheme can communicate directly without using a web server.

There is still a web server; to load content; I always find browser controls are happier if you do not force feed them content against their natural flow and inclinations.

The image view; provides a canvas pane for drawing 2d graphics.

**Limitations**

I am using the single threaded version of scheme so it only ever runs one thing at a time. 

Being linked to the browser control; certain function calls are going to be very asynchronous and possibly run in different threads; even so; only one thing will ever run in the scheme engine at a time.

Some features of Chez Scheme are deeply linked into the core of its terminal loving nature; and very hard to use outside of it; I have tried to redirect the traffic; but not intrusively.  I do not modify Chez Scheme at all.



------

Chez Scheme

https://github.com/cisco/ChezScheme

---

Windows 10 web view for Win 32.

https://docs.microsoft.com/en-us/microsoft-edge/hosting/webview2/gettingstarted

-----


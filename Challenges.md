### Challenges

**First credits**

*It is amazing how little work is required to glue some open source parts together and create something.  I am very impressed also with the fresh approach from Microsoft to things like the new web view control and the new terminal.*

#### There are a few challenges

The three main components are the hosting C++ application; the scheme engine that it hosts; and the web view control that it hosts.

The host application opens a communications channel between the web view and the scheme engine.  There is also a local loop web server in the hosting application; to get the web view control up and running; and feed it content. 

There is code for the UI written in JavaScript and HTML; there is the unmodified binary scheme engine; and the small C++ host application.

#### Threading

The threading model; this is all presently very single threaded; with time sharing between the

browsers event loop and the scheme engine.

The browser will only update; if we speak to it on the UI thread; (like the rest of the Windows GUI); this means that if it was running off the UI thread the scheme engine would need to post messages back via the windows message loop; which is ugly.

The browser posts a message to the hosting C application; and seems to then wait for a response.

The response could probably just be "ok"; and the real response would follow later asynchronously as an entirely new message.

That would allow the scheme engine to have its own threads which would be an improvement; nice to use more than one core.

On a similar topic; I can not get multi-threaded scheme to work at all yet.

Simply swapping single threaded scheme for multithreaded in this app causes a vast number of crashes; even if everything between the host and scheme is wrapped in activate and deactivate thread calls.

Scheme foreign callable; again having trouble getting that to work at all; following the pattern of locking the scheme objects; sending the address to the host and having the host call it back; tends to result in an interesting crash; so for the moment; the calls to scheme are made by name; not by function pointer; and the values passed in have to be scheme objects; not strings.

It is interesting that the web control can spin up multiple windows; that may even (not sure) run in separate processes.  There is some potential concurrency there.

#### Textual messages

The communications between the hosting application and the control appears to be mainly textual. Which is a reasonable starting point.

For example; we send JavaScript text from the host to the web view control for it to evaluate and execute.

I am not seeing an interface that allows us to call a function in the web view control; while providing binary parameters; it seems we are talking to the browser at the moment; but not to the V8 JavaScript engine in the browser directly. There may be a way of doing this; I have not read about yet.

The older browser control based on IE8..11 did allow a function to be called in the browser.

#### Many Languages

- You end up with at least three; there is scheme; which I obviously like as an interactive; yet still efficiently and responsively compiled script language;  there is C++;  and JavaScript+HTML+CSS.
- All of these are going to be involved; and you may wonder how useful each is and what role it might have.
- The scheme engine (apart from being embedded in the C++ host); can itself embed C++ libraries.
- So it can be used to glue a lot of useful libraries together in a composable way; and unlike C++ scheme is very interactive.
- Having the web view as a kind of super advanced user interface (I mean here the whole browser not this little app) is very nice; it is very malleable.
- The fact that the web view also contains its own very fast compiled language is also interesting.



### Strings and text

In C++ strings are a general pain; each significant C++ library;  reinvents something; all of them are very similar.   The std:: library is always being extended or sidestepped and replaced.

What would be nice (and which we do not seem to have) would be a fictional universal 32bit element indexable and mutable string (an array of code points);  with a compressed representation for interchange. 

### Back to reality.

- Unicode needs over 20 bits to encode the code points for the whole worlds character set.
- The western world started out with a 7 bit US specific standard.
- UTF8 extends that with a variable length encoding with code points taking up various numbers of bits; this is almost like; or perhaps it literally is; a kind of text compression format.
- Later there was a 16 bit encoding; that is still not large enough; and that was followed by variable length 16 bit encodings. So ended up being not indexable again.

The three different technologies here are probably using different string formats.

Scheme strings are simplest; they have a large single element per code point.

When Scheme talks to foreign functions it seems to want to use UTF8 for interchange.

Meanwhile windows and the browser control and JavaScript are using 16bit string formats; potentially these are different 16 bit string formats; where either a single or multiple elements represent a code point.

So translation is required between Scheme; Windows; and the Web View control.

Getting that correct will probably be more work; than just getting it working has been.

Above and beyond the strings there are no doubt other issues with character sets that require a great deal of thought and attention; it may become more interesting when the database and excel components are glued in.

### Accessibility

The browser control *presumably* has all the necessary features for that.

The views are all zoomable; which is helpful if you need larger text; CSS styling is flexible.

This is also an area of expertise knowledge and skill.


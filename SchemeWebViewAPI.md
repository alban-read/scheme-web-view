### Scheme Web View simple API  

Between the web view and the host is a communications protocol.

This is presently used as follows; this is the simpler way of doing things; and may get more complicated over time; stuff generally does. 

#### Implementation 

Generally a message is a request from the Web View JavaScript.

**Request:** Web View Control ->  Host -> Scheme

**Reply:** Scheme -> Host -> Web View control

**Example call from JavaScript**

```JavaScript
   function gettranscript() {
        window.chrome.webview.postMessage('::api:1');
    }
```

The post message; requests the transcript text; by sending API call 1.

Response from Scheme

```Scheme
(vector-set! api-calls 1 api-call-get-transcript) ;; this is api call 1.
(define api-call-get-transcript
  (lambda (n s1 )
    (apply string-append 
      (cons ";;; transcript\n" (reverse transcript)))))
```

This is a text protocol:  Every call has an API number that routes it to a function in the Scheme side; via a vector of functions.

A request looks like this.

```
api::n:optional text field follows..
```

A Reply looks like this.

```c++
::api_reply:n:optional text follows on...
```

So far the number and a single value is being used.

The api functions all check; strip and or add the prefix codes.

----------

#### Code for API implementation.

In the JavaScript pages.

A listener is created.

```JavaScript
       window.chrome.webview.addEventListener('message',
            event => handle_events(event)); 
```

A handler is created to process the events.

```javascript
 // unlike a normal web client we have a direct message channel
    //
    function handle_events(event) {

        var text = event.data;

        // evaluator
        var eval_reply = "::eval_reply:";
        if (text.indexOf(eval_reply) !== -1) {
            console.log("eval reply", event);
            result_editor.setValue(text.slice(eval_reply.length));
            gettranscript();
        }

        // api calls.
        var api_reply = "::api_reply:";
        if (text.indexOf(api_reply) !== -1) {
            console.log("api reply", event);


            // api 1 get transcript text response
            if (text.indexOf(":1:") !== -1) {
                transcript_editor.setValue(text.slice(api_reply.length + 2));
            }

            // api 2 clear transcript response
            if (text.indexOf(":2:") !== -1) {
                localStorage.setItem("lastTranscript", '');
                transcript_editor.setValue('');
            }

            // api 3 clear expression response
            if (text.indexOf(":3:") !== -1) {
                localStorage.setItem("lastExpression", '');
                expression_editor.setValue('');
            }

            // api 5 format text response
            if (text.indexOf(":5:") !== -1) {
                response = text.slice(api_reply.length + 2);
                if (response == ';; error response') {
                    result_editor.setValue("Re-Formatted Bad.");
                } else {
                    result_editor.setValue("Re-Formatted Ok.");
                    expression_editor.setValue(response);
                }
            }
        }

        if (text.indexOf("::busy_reply:") !== -1) {
            console.log("busy reply", event);
            result_editor.setValue("scheme engine is busy");
        }
        if (text.indexOf("::invalid_request:") !== -1) {
            console.log("invalid request", event);
            result_editor.setValue("request was not recognized:\n" + text);
        }
    }
```

In the host application in the C++ code; a comms channel talks to the web view control and to the scheme engine. This is implemented as an asynchronous handler.

```C++

// messages between web view 2 and scheme app
// I am sticking with text; and using a prefix in the request and response.
web_view_window->
    add_WebMessageReceived(
    Callback<IWebView2WebMessageReceivedEventHandler>(
        [](IWebView2WebView* webview, 
           IWebView2WebMessageReceivedEventArgs* args) -> HRESULT {

            PWSTR message;
            args->get_WebMessageAsString(&message);

            std::string text = ws_2s(message);
            std::string result;

            // we have one thread at a time in the scheme engine it could be busy.
            if (spin(10))
            {
                webview->PostWebMessageAsString(L"::busy_reply:");
                CoTaskMemFree(message);
                return S_OK;
            }
            try
            {
                // may be an eval message..
                const char* eval_cmd = "::eval:";
                if (text.rfind(eval_cmd, 0) == 0) {

                    // we are going to try and evaluate the message from the browser.
                    const auto scheme_string = 
                        CALL1("eval->string",
                              Sstring(text.c_str()+strlen(eval_cmd)));
                    std::string result;
                    if (scheme_string != Snil && Sstringp(scheme_string))
                    {
                        result = Assoc::Sstring_to_charptr(scheme_string);
                    }
                    std::wstring response;
                    if (result.rfind("::", 0) == std::string::npos)
                        response = L"::eval_reply:";
                    response += s2_ws(result);
                    webview->PostWebMessageAsString(response.c_str());
                    return S_OK;
                }

                // may be an api message..
                const char* api_cmd = "::api:";
                if (text.rfind(api_cmd, 0) == 0) {

                    char* end_ptr;

                    int n = static_cast<int>(strtol(text.c_str() +
                                                    strlen(api_cmd), &end_ptr, 10));
                    std::string param = end_ptr;
                    // browser to scheme api call
                    const auto scheme_string = 
                        CALL2("api-call",
                              Sfixnum(n),
                              Sstring(param.c_str()));
                    std::string result;
                    if (scheme_string != Snil && Sstringp(scheme_string))
                    {
                        result = Assoc::Sstring_to_charptr(scheme_string);
                    }
                    std::wstring response;
                    if (result.rfind("::", 0) == std::string::npos)
                        response = s2_ws(fmt::format("::api_reply:{0}:", n));
                    response += s2_ws(result);
                    webview->PostWebMessageAsString(response.c_str());
                    return S_OK;
                }
            }
            catch (...)
            {

                ReleaseMutex(g_script_mutex);
            }
            std::wstring response = L"::invalid_request:";
            response += message;
            webview->PostWebMessageAsString(response.c_str());
            CoTaskMemFree(message);
            return S_OK;
        }).Get(), &token);

// get this thing on its way
web_view_window->Navigate(navigate_first.c_str());
return S_OK;
}).Get());
```

A message comes in from the browser; the scheme api handler is called; and the response is returned to the browser.

On the scheme side a handler processes API calls like so.

```Scheme

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; api calls from browser window. 

(define api-call-undefined
  (lambda (n s1 )
    (string-append
      "!ERR:api call #"
      (number->string n)
      " is undefined.")))

(define api-call-echo
  (lambda (n s1)
    (string-append (number->string n) ":" s1 )))


(define api-calls (make-vector 64))
(define api-call-limit (vector-length api-calls))
(vector-fill! api-calls api-call-undefined)

(define api-call
  (lambda (n s1 )
    (if (< n api-call-limit)
        (let ([f (vector-ref api-calls n)])
          (if (procedure? f) (apply f (list n s1 )))))))


(define api-call-get-transcript
  (lambda (n s1 )
    (apply string-append 
      (cons ";;; transcript\n" (reverse transcript)))))


(define message-response-get-transcript 
 (lambda ()
   (apply string-append 
      (cons "::transcript_reply:" (reverse transcript)))))


(vector-set! api-calls 1 api-call-get-transcript)

(define api-call-clr-transcript
  (lambda (n s1 ) (clrt) "Ok"))

(vector-set! api-calls 2 api-call-clr-transcript)

(define api-call-clr-expressions
  (lambda (n s1 ) (clrexp) "Ok"))

(vector-set! api-calls 3 api-call-clr-expressions)

(define api-call-get-expressions
  (lambda (n s1 ) (string-append expressions "\n")))

(vector-set! api-calls 4 api-call-get-expressions)

(define api-call-pretty
  (lambda (n s1) (eval->pretty s1)))

(vector-set! api-calls 5 api-call-pretty)

(define api-call-save-evaluator-text
  (lambda (n s1) (save-evaluator-text s1) "Ok"))

(vector-set! api-calls 6 api-call-save-evaluator-text)

```

### So the API is text based and simple.

Thoughts for making it more complicated include; encoding the values as JSON; and mapping them to and from scheme association lists. Implementing the API call into scheme as a foreign callable function rather than calling it by name.

Although the Host to Scheme connection could be made less textual; more binary and more structured; I suspect host to web view control might still be text; need to check into what PostWebMessageAsJSON really means.

Already disappointed that PostWebMessageAsString does not seem to post between threads.


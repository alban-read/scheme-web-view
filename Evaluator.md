### A scheme web view

#### The evaluate function

In scheme an evaluator evaluates the function and returns the results as text.

This is the essence of the entire application; so it is nice that it is straightforward.

```Scheme
(define eval->string
  (lambda (x)
    (define os (open-output-string))
    (try (begin
          (set! expressions x)
           (let* ([is (open-input-string x)])
             (let ([expr (read is)])
               (while
                 (not (eq? #!eof expr))
                 (try (begin (write (eval expr) os) (gc))
                      (catch
                        (lambda (c)
                          (println
                            (call-with-string-output-port
                              (lambda (p) 
                                (display-condition c p)))))))
                 (newline os)
                 (set! expr (read is)))))
           (get-output-string os))
         (catch
           (lambda (c)
             (println
               (call-with-string-output-port
                 (lambda (p) (display-condition c p)))))))))
```

In the web page the JavaScript sends an evaluate message.

```JavaScript
    function evaluateselectedexpression() {
        var text = expression_editor.getSelection();
        window.chrome.webview.postMessage('::eval:' + text);
        localStorage.setItem("lastExpression", text);
    }
```

When shift control is pressed this is called.

The message ::eval: and the text is posted; very like an API call (see api calls.)

In the host C++ application a handler is invoked by the web view control.

```C++
const char* eval_cmd = "::eval:";
if (text.rfind(eval_cmd, 0) == 0) {

 // we are going to try and evaluate the message from the browser.
    const auto scheme_string = CALL1("eval->string", 
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
```

The text from the web view; is sent on to scheme; and the reply from scheme is sent back to the web view.

The web view has an event handler that watches for the eval_reply response.



```JavaScript
// evaluator
var eval_reply = "::eval_reply:";
if (text.indexOf(eval_reply) !== -1) {
    console.log("eval reply", event);
    result_editor.setValue(text.slice(eval_reply.length));
    gettranscript();
}
```

All it needs to do is display the result.

------

Notes:

Although the request and response have been tightly bound like a regular function call; they don't need to be. It make sense that they are; as these are interactive session requests.

A response could be sent later; the initial response might be something like '::ack:eval' instead of the actual response that could be computed in another thread.

This is worth considering; as the scheme side could spin off longer running tasks in another thread; especially when the multithreaded version of scheme is working with this app; if it ever does.

There is not much difference between these evaluate messages and the api calls.

This might be changed over to become api:0 at some point if that makes sense.








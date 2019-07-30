#### Web View commands

#### web-value

```Scheme
;; get web value to scheme value
(web-value "screen.width" "screen-width")
;;
```

- Creates a scheme value  with the current value of the JavaScript function.
- The scheme value is not created immediately; it is created during a callback.

#### web-exec

Call a function in the browser; and run a callback returning the result.

```Scheme
;;
 (define say-hi
  (lambda (name)
    (web-eval (string-append "alert('hello " name "');"))))
;; will call say-hi
(web-exec "prompt('What is your name?');" "say-hi") 
;;
```

- web-exec can define new scripts in the view; you can define entire functions using it.
- The call back is by name of the scheme function you define; first.
- If the name is "" or an invalid name; the callback does not happen.

#### web-eval

Evaluates the script; *just like using eval inside a script*; this returns no results.

```Scheme

```

This is reasonably fast; it is implemented as a message from scheme to the web view; either using *post message as string*; or more quickly and less dangerously using server side events.










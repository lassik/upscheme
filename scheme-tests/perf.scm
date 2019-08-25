(load "test.scm")

(display "colorgraph: ")
(load "tcolor.scm")

(display "fib(34): ")
(assert (equal? (time (fib 34)) 5702887))
(display "yfib(32): ")
(assert (equal? (time (yfib 32)) 2178309))

(display "sort: ")
(set! r (map-int (lambda (x) (mod (+ (* x 9421) 12345) 1024)) 1000))
(time (simple-sort r))

(display "expand: ")
(time (dotimes (n 5000) (expand '(dotimes (i 100) body1 body2))))

(define (my-append . lsts)
  (cond ((null? lsts) ())
        ((null? (cdr lsts)) (car lsts))
        (else (letrec ((append2 (lambda (l d)
                                  (if (null? l) d
                                      (cons (car l)
                                            (append2 (cdr l) d))))))
                (append2 (car lsts) (apply my-append (cdr lsts)))))))

(display "append: ")
(set! L (map-int (lambda (x) (map-int identity 20)) 20))
(time (dotimes (n 1000) (apply my-append L)))

(path.cwd "ast")
(display "p-lambda: ")
(load "rpasses.scm")
(define *input* (load "datetimeR.scm"))
(time (set! *output* (compile-ish *input*)))
(assert (equal? *output* (load "rpasses-out.scm")))
(path.cwd "..")

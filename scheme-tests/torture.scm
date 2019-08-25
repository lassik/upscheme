(define ones (map (lambda (x) 1) (iota 1000000)))

(writeln (apply + ones))

(define (big n)
  (if (<= n 0)
      0
      `(+ 1 1 1 1 1 1 1 1 1 1 ,(big (- n 1)))))

(define nst (big 100000))

(writeln (eval nst))

(define longg (cons '+ ones))
(writeln (eval longg))

(define (f x)
  (begin (writeln x)
         (f (+ x 1))
         0))

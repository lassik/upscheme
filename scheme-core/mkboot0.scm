; -*- scheme -*-

(if (not (bound? 'top-level-value)) (set! top-level-value %eval))
(if (not (bound? 'set-top-level-value!)) (set! set-top-level-value! set))
(if (not (bound? 'eof-object?)) (set! eof-object? (lambda (x) #f)))

(load "dump.scm")
;(load "compiler.scm")

(define (compile-file->buffer inf)
  (let ((in (file inf :read))
        (out (buffer)))
    (let next ((E (read in)))
      (if (not (io.eof? in))
          (begin (write (compile-thunk (expand E)) out)
                 (newline out)
                 (next (read in)))))
    (io.close in)
    (io.seek out 0)
    out))

(dump-buffers-as-c-literal
 (compile-file->buffer "system.scm")
 (compile-file->buffer "compiler.scm"))

(define (dump-buffers-as-c-literal . bufs)
  (princ "char boot_image[] = \"")
  (let loop-bufs ((bufs bufs))
    (if (not (null? bufs))
        (begin (let ((buf (car bufs)))
                 (let loop-buf-bytes ((i 0))
                   (let ((char (read-u8 buf)))
                     (if (not (io.eof? buf))
                         (let ((code (+ char 0)))
                           (if (= 0 (mod i 16)) (princ "\"\n\""))
                           (princ "\\x")
                           (if (< code #x10) (princ "0"))
                           (princ (number->string code 16))
                           (loop-buf-bytes (+ i 1)))))))
               (loop-bufs (cdr bufs)))))
  (princ "\";\n"))

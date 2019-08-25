; -*- scheme -*-

(define (rule30-step b)
  (let ((L (ash b -1))
        (R (ash b 1)))
    (let ((~b (lognot b))
          (~L (lognot L))
          (~R (lognot R)))
      (logior (logand  L ~b ~R)
              (logand ~L  b  R)
              (logand ~L  b ~R)
              (logand ~L ~b  R)))))

(define (bin-draw s)
  (string.map (lambda (c) (case c
                            (#\1 #\#)
                            (#\0 #\ )
                            (else c)))
              s))

(for-each (lambda (n)
            (displayln (bin-draw (string.lpad (number->string n 2) 63 #\0))))
          (nestlist rule30-step (uint64 #x0000000080000000) 32))

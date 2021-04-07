(defun (fib n)
    (cond
        ((= n 0) 0)
        ((= n 1) 1)
        (else (+ (fib (- n 1)) (fib (- n 2))))))

(print "Fibonacci of 3 is " (fib 3))
(print "Fibonacci of 0 is " (fib 0))
(print "Fibonacci of 1 is " (fib 1))
(print "Fibonacci of 15 is " (fib 15))

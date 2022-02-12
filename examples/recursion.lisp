(defun (contains l item)
    (cond
     ((null? l) false)
     ((= (car l) item) true)
     (else (contains (cdr l) item))))

(print "Contains: " (contains '(1 2 3 4 5) 2))
(print "Contains: " (contains '(1 2 3 4 5) 6))
(print "Contains nil: " (contains '(nil "") nil))
(print "Contains: " (contains '(nil "") ""))
(print "Contains: " (contains '(nil "") "  "))

(setq empty-seps '("" nil))
(defun (join strings sep)
    (defun (f acc s)
        (if (not (contains empty-seps sep))
          (+ (+ (to-string s) sep) acc)
          (+ (to-string s) acc)))
    (accumulate f strings ""))

(print "Joined: " (join '("a" "b" "c") " "))
(print "Joined without sep: " (join '(1 2 3 4 5 6)))

(defun (repeat s n)
    (defun (iter k)
      (if (< k 1)
          ""
        (cons s (repeat s (- k 1)))))
    (join (iter n)))

(print "Repeated: " (repeat "a" 30))

(defun (fib n t)
  ;;(print (repeat " " n) n)
  (cond
    ((= n 0) 0)
    ((= n 1) 1)
    (else (+ (fib (- n 1)) (fib (- n 2))))))

(print "Fib is: " (fib 10))

(defun (rec-for n)
  (if (= n 0)
   nil
   (rec-for (- n 1))))

(print "Recursing 500 levels")
(rec-for 500)


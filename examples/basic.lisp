(defun (hello)
  (setq a 5)
  (setq b 8)
  (setq result (+ a b))
  result)

(print "Result of hello() is: " (hello))

(defun (sum a b)
  (setq result (+ a b))
  result)

(print "Result of sum(200, 15) is: " (sum 200 15))

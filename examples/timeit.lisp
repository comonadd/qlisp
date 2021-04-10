(defun (do-some-stuff)
  (sleep 1500))

(print "Running time was " (timeit do-some-stuff))

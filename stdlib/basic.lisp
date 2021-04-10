(defun (case value . cases)
    (if (= cases '())
        nil
        (if (= value nil)
            nil
            (if (= value (car (car cases)))
                (cadr (car cases))
              (case value . (cdr cases))))))

(defun (kilobytes nb)
    (/ nb 1000))

(defun (megabytes nb)
    (/ nb 1000000))

(defun (gigabytes nb)
    (/ nb 1000000000))

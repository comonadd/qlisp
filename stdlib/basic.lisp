(defun (case . conds)
    (if (car (car conds))
        (cadr (car conds))
        (case (cdr conds))))

(set-info :smt-lib-version 2.6)
(set-logic QF_LIA)
(set-info :category "industrial")
(set-info :status sat)
(declare-fun x0 () Int)
(declare-fun x1 () Int)
(declare-fun x2 () Int)
(declare-fun x3 () Int)
(declare-fun x4 () Int)
(declare-fun x5 () Int)
(declare-fun x6 () Int)
(declare-fun x7 () Int)
(declare-fun x8 () Int)
(declare-fun x9 () Int)
(assert (let ((?v_0 (* 0 x6)) (?v_5 (* 1 x4)) (?v_1 (* 1 x8)) (?v_7 (* 1 x5)) (?v_2 (* 0 x7)) (?v_4 (* 1 x2)) (?v_6 (* 1 x0)) (?v_3 (* (- 1) x8))) (and (<= (+ (* (- 1) x0) (+ (* 0 x1) (+ ?v_0 (+ ?v_5 ?v_1)))) 0) (<= (+ (* (- 1) x1) (+ (* (- 1) x3) (+ (* (- 1) x4) (+ ?v_7 (+ ?v_2 (+ ?v_3 (+ (* (- 1) x9) ?v_4))))))) 0) (<= (+ (* 1 x1) (+ (* 0 x3) (+ (* (- 1) x7) (+ (* 0 x4) (+ (* 0 x5) (+ ?v_0 ?v_1)))))) (- 1)) (<= (+ ?v_2 (+ ?v_3 (+ ?v_6 (+ ?v_4 (+ (* 1 x3) ?v_5))))) 0) (<= (+ ?v_6 (+ (* (- 1) x2) (+ ?v_7 (+ (* 2 x6) (* 0 x8))))) 1))))
(check-sat)
(exit)

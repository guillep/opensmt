(set-logic QF_LRA)
(declare-fun r14 () Real)
(push 1)
(check-sat)
(pop 1)
(declare-fun v12 () Bool)
(assert (or (<= 582095008.0 r14) v12))
(assert (and (=> v12 (<= 582095008.0 r14)) (not (<= 582095008.0 r14))))
(push 1)
(check-sat)
(push 1)
(push 1)
(push 1)
(push 1)
(push 1)
(check-sat)
(pop 1)
(pop 1)
(pop 1)
(check-sat)
(pop 1)
(check-sat)
(pop 1)
(pop 1)
(check-sat)
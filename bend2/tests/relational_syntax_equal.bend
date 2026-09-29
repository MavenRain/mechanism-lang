import Base
import ../kernel/bignum.bend as Bignum
import ../surface/syntax.bend as S
import ../surface/universe.bend as U
import ../kernel/quantity.bend as Q
import ../kernel/bignum.bend as N
import ../kernel/common.bend as C

# Exact field-by-field syntax equality for parse/print protocol assertions.
# Every constructor and field in Syntax and Universe participates.
law same_list_2_s_decl:
  for a: List<&2, S.Decl>
  for b: List<&2, S.Decl>
  Bool

law same_s_decl:
  for a: S.Decl
  for b: S.Decl
  Bool

law same_s_t:
  for a: S.T
  for b: S.T
  Bool

law same_u_t:
  for a: U.T
  for b: U.T
  Bool

law same_s_prim:
  for a: S.Prim
  for b: S.Prim
  Bool

law same_list_2_s_t:
  for a: List<&2, S.T>
  for b: List<&2, S.T>
  Bool

law same_list_2_s_binder_s_t:
  for a: List<&2, S.Binder<S.T>>
  for b: List<&2, S.Binder<S.T>>
  Bool

law same_s_binder_s_t:
  for a: S.Binder<S.T>
  for b: S.Binder<S.T>
  Bool

law same_maybe_2_q_t:
  for a: Maybe<&2, Q.T>
  for b: Maybe<&2, Q.T>
  Bool

law same_maybe_2_s_motive_s_t:
  for a: Maybe<&2, S.Motive<S.T>>
  for b: Maybe<&2, S.Motive<S.T>>
  Bool

law same_s_motive_s_t:
  for a: S.Motive<S.T>
  for b: S.Motive<S.T>
  Bool

law same_maybe_2_string:
  for a: Maybe<&2, String>
  for b: Maybe<&2, String>
  Bool

law same_list_2_string:
  for a: List<&2, String>
  for b: List<&2, String>
  Bool

law same_list_2_s_branch_s_t:
  for a: List<&2, S.Branch<S.T>>
  for b: List<&2, S.Branch<S.T>>
  Bool

law same_s_branch_s_t:
  for a: S.Branch<S.T>
  for b: S.Branch<S.T>
  Bool

law same_list_2_s_field_s_t:
  for a: List<&2, S.Field<S.T>>
  for b: List<&2, S.Field<S.T>>
  Bool

law same_s_field_s_t:
  for a: S.Field<S.T>
  for b: S.Field<S.T>
  Bool

law same_maybe_2_s_t:
  for a: Maybe<&2, S.T>
  for b: Maybe<&2, S.T>
  Bool

law same_s_fam_s_t:
  for a: S.Fam<S.T>
  for b: S.Fam<S.T>
  Bool

law same_list_2_s_famctor_s_t:
  for a: List<&2, S.FamCtor<S.T>>
  for b: List<&2, S.FamCtor<S.T>>
  Bool

law same_s_famctor_s_t:
  for a: S.FamCtor<S.T>
  for b: S.FamCtor<S.T>
  Bool

law same_list_2_s_fam_s_t:
  for a: List<&2, S.Fam<S.T>>
  for b: List<&2, S.Fam<S.T>>
  Bool

law same_list_2_s_recdef_s_t:
  for a: List<&2, S.RecDef<S.T>>
  for b: List<&2, S.RecDef<S.T>>
  Bool

law same_s_recdef_s_t:
  for a: S.RecDef<S.T>
  for b: S.RecDef<S.T>
  Bool

law same_list_2_s_specialization:
  for a: List<&2, S.Specialization>
  for b: List<&2, S.Specialization>
  Bool

law same_s_specialization:
  for a: S.Specialization
  for b: S.Specialization
  Bool

law same_list_2_u_t:
  for a: List<&2, U.T>
  for b: List<&2, U.T>
  Bool

law same_list_2_c_pair_string_string:
  for a: List<&2, C.Pair<String, String>>
  for b: List<&2, C.Pair<String, String>>
  Bool

law same_c_pair_string_string:
  for a: C.Pair<String, String>
  for b: C.Pair<String, String>
  Bool

law same_maybe_2_list_2_c_pair_string_string:
  for a: Maybe<&2, List<&2, C.Pair<String, String>>>
  for b: Maybe<&2, List<&2, C.Pair<String, String>>>
  Bool

@unsafe def same_list_2_s_decl(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: same_s_decl(xh, yh) && same_list_2_s_decl(xt, yt)
    case _ _: False{}

@unsafe def same_s_decl(a, b):
  match a b:
    case S.DPoly{x0, x1, x2, x3} S.DPoly{y0, y1, y2, y3}: Nat.is_eq(x0, y0) && String.eq(x1, y1) && same_s_t(x2, y2) && same_s_t(x3, y3)
    case S.DPolyMu{x0, x1} S.DPolyMu{y0, y1}: Nat.is_eq(x0, y0) && same_s_fam_s_t(x1, y1)
    case S.DPolyGroup{x0, x1, x2, x3} S.DPolyGroup{y0, y1, y2, y3}: Nat.is_eq(x0, y0) && same_s_fam_s_t(x1, y1) && same_list_2_s_fam_s_t(x2, y2) && same_list_2_s_recdef_s_t(x3, y3)
    case S.DPolyCompose{x0, x1, x2, x3} S.DPolyCompose{y0, y1, y2, y3}: Nat.is_eq(x0, y0) && String.eq(x1, y1) && same_list_2_s_specialization(x2, y2) && same_list_2_s_recdef_s_t(x3, y3)
    case S.DSpecialize{x0} S.DSpecialize{y0}: same_s_specialization(x0, y0)
    case S.DDef{x0, x1, x2} S.DDef{y0, y1, y2}: String.eq(x0, y0) && same_s_t(x1, y1) && same_s_t(x2, y2)
    case S.DAxiom{x0, x1} S.DAxiom{y0, y1}: String.eq(x0, y0) && same_s_t(x1, y1)
    case S.DMu{x0} S.DMu{y0}: same_list_2_s_fam_s_t(x0, y0)
    case S.DRec{x0} S.DRec{y0}: same_list_2_s_recdef_s_t(x0, y0)
    case _ _: False{}

@unsafe def same_s_t(a, b):
  match a b:
    case S.SVar{x0} S.SVar{y0}: String.eq(x0, y0)
    case S.SNat{x0} S.SNat{y0}: N.equal(x0, y0)
    case S.SProp{} S.SProp{}: True{}
    case S.SType{x0} S.SType{y0}: Bignum.equal(x0, y0)
    case S.SSort{x0} S.SSort{y0}: same_u_t(x0, y0)
    case S.SPrim{x0} S.SPrim{y0}: same_s_prim(x0, y0)
    case S.SUnit{} S.SUnit{}: True{}
    case S.SAuto{} S.SAuto{}: True{}
    case S.SPair{x0, x1} S.SPair{y0, y1}: same_s_t(x0, y0) && same_s_t(x1, y1)
    case S.STuple{x0} S.STuple{y0}: same_list_2_s_t(x0, y0)
    case S.SSum{x0} S.SSum{y0}: same_list_2_s_t(x0, y0)
    case S.SProd{x0} S.SProd{y0}: same_list_2_s_t(x0, y0)
    case S.SProj{x0, x1} S.SProj{y0, y1}: same_s_t(x0, y0) && Bignum.equal(x1, y1)
    case S.SInj{x0, x1, x2} S.SInj{y0, y1, y2}: Bignum.equal(x0, y0) && Bignum.equal(x1, y1) && same_s_t(x2, y2)
    case S.SAbsurd{x0} S.SAbsurd{y0}: same_s_t(x0, y0)
    case S.SApp{x0, x1} S.SApp{y0, y1}: same_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SFun{x0, x1} S.SFun{y0, y1}: same_list_2_s_binder_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SArrow{x0, x1} S.SArrow{y0, y1}: same_s_binder_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SStar{x0, x1} S.SStar{y0, y1}: same_s_binder_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SZkTy{x0, x1} S.SZkTy{y0, y1}: same_s_binder_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SProve{x0, x1, x2} S.SProve{y0, y1, y2}: same_s_t(x0, y0) && same_s_t(x1, y1) && same_s_t(x2, y2)
    case S.SVerify{x0, x1} S.SVerify{y0, y1}: same_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SFhcTy{x0, x1} S.SFhcTy{y0, y1}: same_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SEnc{x0, x1} S.SEnc{y0, y1}: same_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SEval{x0, x1} S.SEval{y0, y1}: same_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SDec{x0, x1} S.SDec{y0, y1}: same_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SMpcTy{x0, x1, x2} S.SMpcTy{y0, y1, y2}: same_s_t(x0, y0) && same_s_t(x1, y1) && same_s_t(x2, y2)
    case S.SShare{x0} S.SShare{y0}: same_s_t(x0, y0)
    case S.SInput{x0, x1} S.SInput{y0, y1}: same_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SJoin{x0, x1, x2} S.SJoin{y0, y1, y2}: same_s_t(x0, y0) && same_s_t(x1, y1) && same_list_2_s_t(x2, y2)
    case S.SOpen{x0, x1, x2, x3} S.SOpen{y0, y1, y2, y3}: same_maybe_2_q_t(x0, y0) && same_s_t(x1, y1) && same_s_t(x2, y2) && same_s_t(x3, y3)
    case S.SLet{x0, x1, x2, x3} S.SLet{y0, y1, y2, y3}: String.eq(x0, y0) && same_s_t(x1, y1) && same_s_t(x2, y2) && same_s_t(x3, y3)
    case S.SAnn{x0, x1} S.SAnn{y0, y1}: same_s_t(x0, y0) && same_s_t(x1, y1)
    case S.SCase{x0, x1, x2} S.SCase{y0, y1, y2}: same_s_t(x0, y0) && same_maybe_2_s_motive_s_t(x1, y1) && same_list_2_s_branch_s_t(x2, y2)
    case S.SMatch{x0, x1, x2} S.SMatch{y0, y1, y2}: same_s_t(x0, y0) && same_maybe_2_s_motive_s_t(x1, y1) && same_list_2_s_branch_s_t(x2, y2)
    case _ _: False{}

@unsafe def same_u_t(a, b):
  match a b:
    case U.NatLevel{x0} U.NatLevel{y0}: Bignum.equal(x0, y0)
    case U.VarLevel{x0} U.VarLevel{y0}: Nat.is_eq(x0, y0)
    case U.SuccLevel{x0} U.SuccLevel{y0}: same_u_t(x0, y0)
    case U.MaxLevel{x0, x1} U.MaxLevel{y0, y1}: same_u_t(x0, y0) && same_u_t(x1, y1)
    case U.IMaxLevel{x0, x1} U.IMaxLevel{y0, y1}: same_u_t(x0, y0) && same_u_t(x1, y1)
    case _ _: False{}

@unsafe def same_s_prim(a, b):
  match a b:
    case S.PAdd{} S.PAdd{}: True{}
    case S.PSub{} S.PSub{}: True{}
    case S.PMul{} S.PMul{}: True{}
    case S.PEq{} S.PEq{}: True{}
    case S.PLt{} S.PLt{}: True{}
    case _ _: False{}

@unsafe def same_list_2_s_t(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: same_s_t(xh, yh) && same_list_2_s_t(xt, yt)
    case _ _: False{}

@unsafe def same_list_2_s_binder_s_t(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: same_s_binder_s_t(xh, yh) && same_list_2_s_binder_s_t(xt, yt)
    case _ _: False{}

@unsafe def same_s_binder_s_t(a, b):
  match a b:
    case S.Binder{x0, x1, x2} S.Binder{y0, y1, y2}: Q.equal(x0, y0) && String.eq(x1, y1) && same_s_t(x2, y2)

@unsafe def same_maybe_2_q_t(a, b):
  match a b:
    case None{} None{}: True{}
    case Some{x} Some{y}: Q.equal(x, y)
    case _ _: False{}

@unsafe def same_maybe_2_s_motive_s_t(a, b):
  match a b:
    case None{} None{}: True{}
    case Some{x} Some{y}: same_s_motive_s_t(x, y)
    case _ _: False{}

@unsafe def same_s_motive_s_t(a, b):
  match a b:
    case S.Motive{x0, x1, x2, x3} S.Motive{y0, y1, y2, y3}: String.eq(x0, y0) && same_maybe_2_string(x1, y1) && same_list_2_string(x2, y2) && same_s_t(x3, y3)

@unsafe def same_maybe_2_string(a, b):
  match a b:
    case None{} None{}: True{}
    case Some{x} Some{y}: String.eq(x, y)
    case _ _: False{}

@unsafe def same_list_2_string(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: String.eq(xh, yh) && same_list_2_string(xt, yt)
    case _ _: False{}

@unsafe def same_list_2_s_branch_s_t(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: same_s_branch_s_t(xh, yh) && same_list_2_s_branch_s_t(xt, yt)
    case _ _: False{}

@unsafe def same_s_branch_s_t(a, b):
  match a b:
    case S.BrLeg{x0, x1, x2} S.BrLeg{y0, y1, y2}: Bignum.equal(x0, y0) && same_list_2_s_binder_s_t(x1, y1) && same_s_t(x2, y2)
    case S.BrCtor{x0, x1, x2} S.BrCtor{y0, y1, y2}: String.eq(x0, y0) && same_list_2_s_field_s_t(x1, y1) && same_s_t(x2, y2)
    case _ _: False{}

@unsafe def same_list_2_s_field_s_t(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: same_s_field_s_t(xh, yh) && same_list_2_s_field_s_t(xt, yt)
    case _ _: False{}

@unsafe def same_s_field_s_t(a, b):
  match a b:
    case S.Field{x0, x1, x2} S.Field{y0, y1, y2}: Q.equal(x0, y0) && String.eq(x1, y1) && same_maybe_2_s_t(x2, y2)

@unsafe def same_maybe_2_s_t(a, b):
  match a b:
    case None{} None{}: True{}
    case Some{x} Some{y}: same_s_t(x, y)
    case _ _: False{}

@unsafe def same_s_fam_s_t(a, b):
  match a b:
    case S.Fam{x0, x1, x2, x3} S.Fam{y0, y1, y2, y3}: String.eq(x0, y0) && same_list_2_s_binder_s_t(x1, y1) && same_s_t(x2, y2) && same_list_2_s_famctor_s_t(x3, y3)

@unsafe def same_list_2_s_famctor_s_t(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: same_s_famctor_s_t(xh, yh) && same_list_2_s_famctor_s_t(xt, yt)
    case _ _: False{}

@unsafe def same_s_famctor_s_t(a, b):
  match a b:
    case S.FamCtor{x0, x1} S.FamCtor{y0, y1}: String.eq(x0, y0) && same_s_t(x1, y1)

@unsafe def same_list_2_s_fam_s_t(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: same_s_fam_s_t(xh, yh) && same_list_2_s_fam_s_t(xt, yt)
    case _ _: False{}

@unsafe def same_list_2_s_recdef_s_t(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: same_s_recdef_s_t(xh, yh) && same_list_2_s_recdef_s_t(xt, yt)
    case _ _: False{}

@unsafe def same_s_recdef_s_t(a, b):
  match a b:
    case S.RecDef{x0, x1, x2} S.RecDef{y0, y1, y2}: String.eq(x0, y0) && same_s_t(x1, y1) && same_s_t(x2, y2)

@unsafe def same_list_2_s_specialization(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: same_s_specialization(xh, yh) && same_list_2_s_specialization(xt, yt)
    case _ _: False{}

@unsafe def same_s_specialization(a, b):
  match a b:
    case S.Specialization{x0, x1, x2, x3, x4} S.Specialization{y0, y1, y2, y3, y4}: String.eq(x0, y0) && same_list_2_u_t(x1, y1) && String.eq(x2, y2) && same_list_2_c_pair_string_string(x3, y3) && same_maybe_2_list_2_c_pair_string_string(x4, y4)

@unsafe def same_list_2_u_t(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: same_u_t(xh, yh) && same_list_2_u_t(xt, yt)
    case _ _: False{}

@unsafe def same_list_2_c_pair_string_string(a, b):
  match a b:
    case Nil{} Nil{}: True{}
    case Con{xh, xt} Con{yh, yt}: same_c_pair_string_string(xh, yh) && same_list_2_c_pair_string_string(xt, yt)
    case _ _: False{}

@unsafe def same_c_pair_string_string(a, b):
  match a b:
    case C.Pair{x0, x1} C.Pair{y0, y1}: String.eq(x0, y0) && String.eq(x1, y1)

@unsafe def same_maybe_2_list_2_c_pair_string_string(a, b):
  match a b:
    case None{} None{}: True{}
    case Some{x} Some{y}: same_list_2_c_pair_string_string(x, y)
    case _ _: False{}

def declarations(a: List<&2, S.Decl>, b: List<&2, S.Decl>) -> Bool:
  same_list_2_s_decl(a, b)

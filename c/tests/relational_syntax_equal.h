/* Port of bend2/tests/relational_syntax_equal.bend: exact field-by-field syntax equality for the
 * parse/print protocol assertions. Every constructor and field in Syntax and Universe participates.
 * A library module (no main): the S2.4 surface drivers link it. */
#ifndef MECH_RELATIONAL_SYNTAX_EQUAL_H
#define MECH_RELATIONAL_SYNTAX_EQUAL_H

#include "surface.h"

bool syntax_equal_same_list_2_s_decl(const Vec *a, const Vec *b);
bool syntax_equal_same_s_decl(const SDecl *a, const SDecl *b);
bool syntax_equal_same_s_t(const STerm *a, const STerm *b);
bool syntax_equal_same_u_t(const Universe *a, const Universe *b);
bool syntax_equal_same_s_prim(SPrim a, SPrim b);
bool syntax_equal_same_list_2_s_t(const Vec *a, const Vec *b);
bool syntax_equal_same_list_2_s_binder_s_t(const Vec *a, const Vec *b);
bool syntax_equal_same_s_binder_s_t(SBinder a, SBinder b);
bool syntax_equal_same_maybe_2_q_t(MaybeQuantity a, MaybeQuantity b);
bool syntax_equal_same_maybe_2_s_motive_s_t(MaybeSMotive a, MaybeSMotive b);
bool syntax_equal_same_s_motive_s_t(SMotive a, SMotive b);
bool syntax_equal_same_maybe_2_string(MaybeStr a, MaybeStr b);
bool syntax_equal_same_list_2_string(const Vec *a, const Vec *b);
bool syntax_equal_same_list_2_s_branch_s_t(const Vec *a, const Vec *b);
bool syntax_equal_same_s_branch_s_t(const SBranch *a, const SBranch *b);
bool syntax_equal_same_list_2_s_field_s_t(const Vec *a, const Vec *b);
bool syntax_equal_same_s_field_s_t(const SField *a, const SField *b);
bool syntax_equal_same_maybe_2_s_t(MaybeSTerm a, MaybeSTerm b);
bool syntax_equal_same_s_fam_s_t(const SFam *a, const SFam *b);
bool syntax_equal_same_list_2_s_famctor_s_t(const Vec *a, const Vec *b);
bool syntax_equal_same_s_famctor_s_t(const SFamCtor *a, const SFamCtor *b);
bool syntax_equal_same_list_2_s_fam_s_t(const Vec *a, const Vec *b);
bool syntax_equal_same_list_2_s_recdef_s_t(const Vec *a, const Vec *b);
bool syntax_equal_same_s_recdef_s_t(const SRecDef *a, const SRecDef *b);
bool syntax_equal_same_list_2_s_specialization(const Vec *a, const Vec *b);
bool syntax_equal_same_s_specialization(const SSpecialization *a, const SSpecialization *b);
bool syntax_equal_same_list_2_u_t(const Vec *a, const Vec *b);
bool syntax_equal_same_list_2_c_pair_string_string(const Vec *a, const Vec *b);
bool syntax_equal_same_c_pair_string_string(const PolyRename *a, const PolyRename *b);
bool syntax_equal_same_maybe_2_list_2_c_pair_string_string(MaybeVec a, MaybeVec b);
bool syntax_equal_declarations(const Vec *a, const Vec *b);

#endif

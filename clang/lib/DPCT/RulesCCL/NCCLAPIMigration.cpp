//===------------------- NCCLAPIMigration.cpp ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------------===//

#include "NCCLAPIMigration.h"
#include "RuleInfra/ExprAnalysis.h"

using namespace clang::dpct;
using namespace clang::ast_matchers;

void clang::dpct::NCCLRule::registerMatcher(ast_matchers::MatchFinder &MF) {
  MF.addMatcher(typeLoc(loc(qualType(hasDeclaration(namedDecl(hasAnyName(
                            "ncclUniqueId", "ncclComm_t", "ncclRedOp_t",
                            "ncclDataType_t", "ncclResult_t"))))))
                    .bind("type"),
                this);
  MF.addMatcher(
      callExpr(callee(functionDecl(hasAnyName(
                   "ncclGetVersion", "ncclGetUniqueId", "ncclCommInitRank",
                   "ncclCommCount", "ncclCommCuDevice", "ncclAllReduce",
                   "ncclCommUserRank", "ncclBroadcast", "ncclCommDestroy",
                   "ncclReduce", "ncclReduceScatter", "ncclBcast", "ncclSend",
                   "ncclRecv", "ncclGetErrorString", "ncclGetLastError",
                   "ncclCommGetAsyncError"))))
          .bind("call"),
      this);
  MF.addMatcher(
      declRefExpr(to(enumConstantDecl(hasAnyName(
                      "ncclChar", "ncclUint8", "ncclInt32", "ncclInt",
                      "ncclUint32", "ncclInt64", "ncclUint64", "ncclFloat16",
                      "ncclHalf", "ncclFloat32", "ncclFloat", "ncclFloat64",
                      "ncclDouble", "ncclBfloat16", "ncclSum", "ncclProd",
                      "ncclMin", "ncclMax", "ncclSuccess"))))
          .bind("enum"),
      this);
}

void clang::dpct::NCCLRule::runRule(
    const ast_matchers::MatchFinder::MatchResult &Result) {
  ExprAnalysis EA;
  if (const auto *TL = getNodeAsType<TypeLoc>(Result, "type")) {
    EA.analyze(*TL);
  } else if (const CallExpr *CE = getNodeAsType<CallExpr>(Result, "call")) {
    EA.analyze(CE);
  } else if (const DeclRefExpr *DRE =
                 getNodeAsType<DeclRefExpr>(Result, "enum")) {
    EA.analyze(DRE);
    const auto *ICE = DpctGlobalInfo::findParent<clang::ImplicitCastExpr>(DRE);
    if (ICE != nullptr) {
      std::string ReplStr = EA.getReplacedString();
      if (ReplStr.find("oneapi::ccl::", 0) == 0) {
        emplaceTransformation(
            new ReplaceStmt(DRE, DpctGlobalInfo::getTypeName(ICE->getType()) +
                                     "(" + std::string(ReplStr) + ")"));
        EA.applyAllSubExprRepl();
        return;
      }
    }

  } else {
    return;
  }
  emplaceTransformation(EA.getReplacement());
  EA.applyAllSubExprRepl();
}

void ManualMigrateEnumsRule::registerMatcher(MatchFinder &MF) {
  MF.addMatcher(declRefExpr(to(enumConstantDecl(matchesName("NCCL_.*"))))
                    .bind("NCCLConstants"),
                this);
}

void ManualMigrateEnumsRule::runRule(const MatchFinder::MatchResult &Result) {
  if (const DeclRefExpr *DE =
          getNodeAsType<DeclRefExpr>(Result, "NCCLConstants")) {
    auto *ECD = cast<EnumConstantDecl>(DE->getDecl());
    if (DpctGlobalInfo::isInAnalysisScope(ECD->getBeginLoc())) {
      return;
    }
    report(dpct::DpctGlobalInfo::getSourceManager().getExpansionLoc(
               DE->getBeginLoc()),
           Diagnostics::MANUAL_MIGRATION_LIBRARY, false,
           "Intel(R) oneAPI Collective Communications Library");
  }
}

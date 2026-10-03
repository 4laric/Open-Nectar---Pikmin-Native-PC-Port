# Original body yaw arithmetic

Issue 4laric/pikmin-randomizer#1261; implementation owner Codex through shared
account 4laric. This is pure arithmetic, independent of live body admission.

`pc_p2_original_number_double` performs finite binary64 fused arithmetic with
integer significands and one nearest/even rounding. It refuses unsupported
rounding, flush-to-zero and nonfinite inputs/results without changing output.
`pc_p2_original_number_trig` implements the bounded original JMath initializer,
including the original MSL binary64 fused instruction schedule. It generates all
2048 sine/cosine pairs from exact binary32 TAU, then performs the original
binary32 multiply, truncating signed-int conversion and 2047 mask. Oversized
conversions refuse. No host sin, cos or FMA is used.

`pc_p2_original_number_yaw` preserves original unit-scale FakePiki/Matrixf SRT
operations. Root world position is the translation column of SDK PSMTXConcat:
rounded X product, Y fused term, Z fused term, then body translation fused term.
The root matrix's other columns do not contribute to its translation column.
The header-only `SourceRootTransform` implements the parent's exact
`p2original::piki::SourceRootWorldTransform` interface. Compose that header only
alongside the genuine Piki animator/runtime provider; no substitute interface is
defined here. The parent authenticates actual RGB unit scale, body transform,
dense root, animation clock, selected resources and lifetime before and after
the callback. Arithmetic success alone grants none of those properties.

Original GPVE01 revision 0 function pins:

| Routine | Address / bytes | SHA-256 |
| --- | --- | --- |
| JMath initializer | 80035360 / 1c0 | 5abb57641ffd04f954c305f3ccd824393eb7c6f4cd5a8e7071fafadb851d6878 |
| rem_pio2 | 800cd994 / 3a0 | ee3188edfeadf942fbe09770ca6d4bcf36a7bc854da352409aa87df75b326124 |
| kernel_sin | 800cec7c / a0 | ab25a600b69e1c64684d2f4dd5faded0eefbb324acce17ddd5383a6c54f16285 |
| kernel_cos | 800cdd34 / f4 | a6241b5e4a5d02c0e8433bd440551d317d2eb3ab68aa25706fb9818f27a2423b |
| sin | 800cf81c / d8 | a5d0399a52f3d97d01a1e9f4b7a558162b28d132b42d36ac2c63420f9ab5a30c |
| cos | 800cf2b4 / d4 | 24c8e82cf26e6f9a6d0e530e50a253afaa8b15bb5dc5fd1b329dbb58b36df3b1 |
| Matrixf::makeSRT | 804282d8 / 288 | 863fb87c29bdee1d1b7b9024919506a3ca6c0a8884dec784f327a6dbe7e1cab8 |
| FakePiki::updateTrMatrix | 8013ed50 / 78 | 849bd819b3b152e94b2b01e0687839dae1bd6fc320e64feb9bd129767626ed11 |
| PSMTXConcat | 800ea300 / cc | a7187d392afbc501f09ea521da09aaa9f0c624bc9a276b60cae732b99b8a8168 |

The initializer's actual domain requires no large reduction; only entries 1024
and 1536 take the second medium correction. This is a qualified portable
expression of the audited instructions, not a retained original hardware table
or floating-point status-register capture. The Sun license notice is preserved.

Default CTests cover literal double/transform controls and all 4096 table
components against checked-in independent Fraction-oracle goldens. Regenerate
the broader controls in a private build directory, using Python 3:

```text
python tools/p2_original_number_double_oracle.py <build>/double-goldens.tsv
python tools/p2_original_number_trig_oracle.py <build>/oracle-table.bin
python tools/p2_original_number_yaw_oracle.py <build>/oracle-table.bin <build>/body-yaw-goldens.tsv
<build>/p2_original_number_double_test <build>/double-goldens.tsv
<build>/p2_original_number_trig_test <build>/oracle-table.bin
<build>/p2_original_number_yaw_test <build>/body-yaw-goldens.tsv
```

The generated table's little-endian SHA-256 is
`2d2d4f4bfc49d8884ca357b7aebb6083394229d10ef32c46c14973e4045503d0`.
These checks do not qualify actual owner composition, World/Cell admission,
hardware equivalence, gameplay, or SAVE/resume.

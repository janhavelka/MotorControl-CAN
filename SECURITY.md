# Security policy

MotorControl-CAN is a development package with no published supported release
series. Version metadata identifies source; it does not promise a maintenance
window, certification or a response deadline. The current installed core
validates local endpoint identity and Classical CAN frame shape. It performs
no network I/O, drive commands or authentication.

## Reporting

No verified private vulnerability reporting channel is established yet. This
remains a release prerequisite. For a non-sensitive defect, use the
[repository issue tracker](https://github.com/janhavelka/MotorControl-CAN/issues)
with the affected commit/version, build toolchain, minimal reproducer and
observed versus expected behavior. Do not post credentials, private network
details or an exploit requiring coordinated disclosure in a public issue.
This policy does not imply that GitHub private vulnerability reporting is
enabled or that a private address exists.

## Scope and integration responsibilities

Treat externally supplied identifiers, lengths, enum values, metadata and
protocol replies as untrusted. Public functions document their validation
scope and failure publication. Report overflow, out-of-bounds access,
unexpected allocation, resource-bound violations and acceptance of malformed
data as defects. A successful local validation does not establish device
identity, drive acceptance, motion completion or safe physical behavior.

Applications own CAN controllers, receive dispatch, binding/configuration
freshness, scheduling, transport security, retries and machine controls.
Future CANopen/drive services require their own protocol and device evidence.
CAN traffic and a host library alone do not provide a safety system. Retain the
precise firmware, fixture, traffic and timing needed for a hardware finding;
do not replay an uncertain motor write merely to reproduce a software report.

Original vendor documents and optional dependencies retain their separate
licenses and reporting policies. See [LICENSE](LICENSE), the
[source manifests](docs/reference/README.md) and the
[architecture boundary](docs/architecture.md).

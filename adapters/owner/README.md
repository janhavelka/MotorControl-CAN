# Application CAN owner

Optional target `MotorControlCAN::CanBusOwner`, namespace `MotorControlCANOwner`. It schedules one CAN interface's service work over the [CANopenNode adapter](../canopennode/README.md) and is not installed with the portable core, which stays free of stack and SDK headers. Enable it with `MOTORCONTROLCAN_BUILD_CANOPENNODE`.

A controller adapter such as [TWAI](../twai/README.md) supplies its `Transport`. Applications, consoles and board code stay in `examples/`. The [owner guide](../../docs/can_owner.md) gives the contract.

The pinned CANopenNode copy stays in `experiments/canopen_service_fit/vendor` for now; prompt 49 owns moving it for packaging.

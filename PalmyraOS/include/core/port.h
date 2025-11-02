#pragma once

#include "core/definitions.h"


namespace PalmyraOS::kernel::ports {
    class BasePort {
    public:
        explicit BasePort(uint16_t portNumber);
        ~BasePort()                                 = default;

        virtual void write(uint32_t data) const     = 0;
        [[nodiscard]] virtual uint32_t read() const = 0;

    protected:
        uint16_t portNumber_;
    };

    class BytePort : public BasePort {
    public:
        using BasePort::BasePort;
        void write(uint32_t data) const override;
        [[nodiscard]] uint32_t read() const override;
    };

    class SlowBytePort : public BytePort {
    public:
        using BytePort::BytePort;
        void write(uint32_t data) const override;
    };

    class WordPort : public BasePort {
    public:
        using BasePort::BasePort;
        void write(uint32_t data) const override;
        [[nodiscard]] uint32_t read() const override;
    };

    class DoublePort : public BasePort {
    public:
        using BasePort::BasePort;
        void write(uint32_t data) const override;
        [[nodiscard]] uint32_t read() const override;
    };

}  // namespace PalmyraOS::kernel::ports

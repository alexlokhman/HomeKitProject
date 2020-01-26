
#pragma once

#include <SoftwareSerial.h>

#define   COMMAND_MARKER_START          '<'
#define   COMMAND_MARKER_END            '>'
#define   COMMAND_PARAM_DELIMITER       '|'

template<uint8_t PacketMarkerStart = COMMAND_MARKER_START, 
          uint8_t PacketMarkerEnd = COMMAND_MARKER_END, 
          uint8_t PacketParamDelimiter = COMMAND_PARAM_DELIMITER, 
          size_t ReceiveBufferSize = 256, 
          size_t MaxParameters = 10>

class PacketSerial_
{
  public:
    /// \brief A typedef describing the packet handler method.
    ///
    /// The packet handler method usually has the form:
    ///
    ///     void onPacketReceived(const uint8_t* buffer[], size_t size);
    ///
    /// where buffer is a pointer to the incoming buffer array, and size is the
    /// number of bytes in the incoming buffer.
    typedef void (*PacketHandlerFunction)(const uint8_t* buffer[], size_t size);


    /// \brief Construct a default PacketSerial_ device.
    PacketSerial_():
        _receiveBufferIndex(0),
        _stream(nullptr),
        _onPacketFunction(nullptr)
    {
    }

    /// \brief Destroy the PacketSerial_ device.
    ~PacketSerial_()
    {
    }

    /// \brief Attach PacketSerial to an existing Arduino `Stream`.
    ///
    /// This `Stream` could be a standard `Serial` `Stream` with a non-default
    /// configuration such as:
    ///
    ///     PacketSerial myPacketSerial;
    ///
    ///     void setup()
    ///     {
    ///         Serial.begin(300, SERIAL_7N1);
    ///         myPacketSerial.setStream(&Serial);
    ///     }
    ///
    /// Or it might be a `SoftwareSerial` `Stream` such as:
    ///
    ///     PacketSerial myPacketSerial;
    ///     SoftwareSerial mySoftwareSerial(10, 11);
    ///
    ///     void setup()
    ///     {
    ///         mySoftwareSerial.begin(38400);
    ///         myPacketSerial.setStream(&mySoftwareSerial);
    ///     }
    ///
    /// Any class that implements the `Stream` interface should work, which
    /// includes some network objects.
    ///
    /// \param stream A pointer to an Arduino `Stream`.
    void setStream(Stream* stream)
    {
        _stream = stream;
    }

    /// \brief Get a pointer to the current stream.
    /// \warning Reading from or writing to the stream managed by PacketSerial_
    ///          may break the packet-serial protocol if not done so with care. 
    ///          Access to the stream is allowed because PacketSerial_ never
    ///          takes ownership of the stream and thus does not have exclusive
    ///          access to the stream anyway.
    /// \returns a non-const pointer to the stream, or nullptr if unset.
    Stream* getStream()
    {
        return _stream;
    }

    /// \brief Get a pointer to the current stream.
    /// \warning Reading from or writing to the stream managed by PacketSerial_
    ///          may break the packet-serial protocol if not done so with care. 
    ///          Access to the stream is allowed because PacketSerial_ never
    ///          takes ownership of the stream and thus does not have exclusive
    ///          access to the stream anyway.
    /// \returns a const pointer to the stream, or nullptr if unset.
    const Stream* getStream() const
    {
        return _stream;
    }

    /// \brief The update function services the serial connection.
    ///
    /// This must be called often, ideally once per `loop()`, e.g.:
    ///
    ///     void loop()
    ///     {
    ///         // Other program code.
    ///
    ///         myPacketSerial.update();
    ///     }
    ///
    void update()
    {
        if (_stream == nullptr) return;

        while (_stream->available() > 0)
        {
            uint8_t data = _stream->read();

            if (data == PacketMarkerStart) {
              _receivingPacket = true;
              _receiveBufferIndex = 0;
              _recieveBufferOverflow = false;
            }
            else if (data == PacketMarkerEnd)
            {
                if (_receivingPacket) {
                  if (_onPacketFunction)
                  {
                      uint8_t _decodeBuffer[_receiveBufferIndex+1];
                      uint8_t* _paramsBuffer[MaxParameters];

                      size_t numParams = 0;
                       _paramsBuffer[0] = &_decodeBuffer[0];
                       
                      for (size_t i=0; i<_receiveBufferIndex; i++) {
                        if (_receiveBuffer[i] == PacketParamDelimiter) {
                          _decodeBuffer[i] = '\0';
                          numParams++;
                          _paramsBuffer[numParams] = &_decodeBuffer[i+1];
                        }
                        else
                          _decodeBuffer[i] = _receiveBuffer[i];
                      }
                      _decodeBuffer[_receiveBufferIndex] = '\0';
                                                                                    
                      _onPacketFunction(_paramsBuffer, min(numParams+1, MaxParameters));
                  }
  
                  _receiveBufferIndex = 0;
                  _recieveBufferOverflow = false;
                  _receivingPacket = false;
                }
            }
            else {
              if (_receivingPacket) {
                if ((_receiveBufferIndex + 1) < ReceiveBufferSize)
                {
                    _receiveBuffer[_receiveBufferIndex++] = data;
                }
                else
                {
                    // The buffer will be in an overflowed state if we write
                    // so set a buffer overflowed flag.
                    _recieveBufferOverflow = true;
                }         
              }     
            }
            
        }
    }


    /// \brief Set a packet of data.
    ///
    /// This function will encode and send an arbitrary packet of data. After
    /// sending, it will send the specified `PacketMarker` defined in the
    /// template parameters.
    ///
    ///     // Make an array.
    ///     uint8_t myPacket[2] = { 255, 10 };
    ///
    ///     // Send the array.
    ///     myPacketSerial.send(myPacket, 2);
    ///
    /// \param buffer A pointer to a data buffer.
    /// \param size The number of bytes in the data buffer.
    void send(const uint8_t* buffer, size_t size) const    
    {
        if(_stream == nullptr || buffer == nullptr || size == 0) return;

        uint8_t _encodeBuffer[size];

        /*
        size_t numEncoded = EncoderType::encode(buffer,
                                                size,
                                                _encodeBuffer);
                                                */
        size_t numEncoded = size;

        _stream->write(PacketMarkerStart);
        _stream->write(_encodeBuffer, numEncoded);
        _stream->write(PacketMarkerEnd);
    }

    void send(const char* cmd, ...) const {
      if(_stream == nullptr || cmd == nullptr) return;
      
      char _buf[ReceiveBufferSize];
      memset(_buf, 0, ReceiveBufferSize);

      _buf[0] = PacketMarkerStart;

      char *b;
      va_list ap;                         /* Parameter list reference            */  
      va_start(ap, cmd);                  /* Initialize ap                       */      
      strcat(_buf, cmd);      
      
      while (b = va_arg(ap,char *)) {     /* If the next parameter is non-NULL   */
        _buf[strlen(_buf)] = PacketParamDelimiter;
        strcat(_buf, b);
      }  
      va_end(ap);                         /* Done with parameter list            */
      _buf[strlen(_buf)] = PacketMarkerEnd;
    
      _stream->write(_buf, strlen(_buf));
    }

    /// \brief Set the function that will receive decoded packets.
    ///
    /// This function will be called when data is read from the serial stream
    /// connection and a packet is decoded. The decoded packet will be passed
    /// to the packet handler. The packet handler must have the form:
    ///
    /// The packet handler method usually has the form:
    ///
    ///     void onPacketReceived(const uint8_t* buffer, size_t size);
    ///
    /// The packet handler would then be registered like this:
    ///
    ///     myPacketSerial.setPacketHandler(&onPacketReceived);
    ///
    /// Setting a packet handler will remove all other packet handlers.
    ///
    /// \param onPacketFunction A pointer to the packet handler function.
    void setPacketHandler(PacketHandlerFunction onPacketFunction)
    {
        _onPacketFunction = onPacketFunction;
    }            

    /// \brief Check to see if the receive buffer overflowed.
    ///
    /// This must be called often, directly after the `update()` function.
    ///
    ///     void loop()
    ///     {
    ///         // Other program code.
    ///         myPacketSerial.update();
    ///
    ///         // Check for a receive buffer overflow.
    ///         if (myPacketSerial.overflow())
    ///         {
    ///             // Send an alert via a pin (e.g. make an overflow LED) or return a
    ///             // user-defined packet to the sender.
    ///             //
    ///             // Ultimately you may need to just increase your recieve buffer via the
    ///             // template parameters.
    ///         }
    ///     }
    ///
    /// The state is reset every time a new packet marker is received NOT when 
    /// overflow() method is called.
    ///
    /// \returns true if the receive buffer overflowed.
    bool overflow() const
    {
        return _recieveBufferOverflow;
    }

private:
    PacketSerial_(const PacketSerial_&);
    PacketSerial_& operator = (const PacketSerial_&);

    bool _recieveBufferOverflow = false;
    bool _receivingPacket = false;

    uint8_t _receiveBuffer[ReceiveBufferSize];
    size_t _receiveBufferIndex = 0;

    Stream* _stream = nullptr;

    PacketHandlerFunction _onPacketFunction = nullptr;
};


/// \brief A typedef for the default ASCII PacketSerial class.
typedef PacketSerial_<COMMAND_MARKER_START, COMMAND_MARKER_END, COMMAND_PARAM_DELIMITER> PacketSerial;

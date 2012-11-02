﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace HefnyCopter.CommunicationProtocol
{

    public enum ENUM_RxDataType
    {
        Undefined,
        Sensors,
        Settings
    }


    public class HefnyCopterBaseConnection
    {
       

        public delegate void delegate_CopyData(ENUM_RxDataType DataType , byte[] DataArray);


        #region "Attributes"


        public delegate_CopyData mdelegate_CopyData;

        protected string mReceivedData;

        #endregion 



        #region "Properties"
        #endregion 



        #region "Constructors"

        protected HefnyCopterBaseConnection()
        {
        }

        #endregion 


        #region "Methods"


        public virtual void Open()
        {
            throw new NotImplementedException();
        }


        public virtual void Close()
        {
            throw new NotImplementedException();
        }


        protected virtual void CopyData(ENUM_RxDataType DataType, byte[] vArray)
        {
            throw new NotImplementedException();
        }

        #endregion

    }
}

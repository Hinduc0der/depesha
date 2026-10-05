#include "tgclient.h"

#include "apisecrets.h"
#include "tlschema.h"

TgLongVariant TgClient::authSendCode(QString phoneNumber)
{
    TGOBJECT(TLType::AuthSendCodeMethod, method);

    TGOBJECT(TLType::CodeSettings, codeSettings);
    method["settings"] = codeSettings;

    method["phone_number"] = phoneNumber;

#if defined(KUTEGRAM_API_ID)
    method["api_id"] = KUTEGRAM_API_ID;
#else
    #error "Please, specify an API id."
#endif

#if defined(KUTEGRAM_API_HASH)
    method["api_hash"] = KUTEGRAM_API_HASH;
#else
    #error "Please, specify an API hash."
#endif

    return sendObject<&writeTLMethodAuthSendCode>(method);
}

TgLongVariant TgClient::authSignIn(QString phoneNumber, QString phoneCodeHash, QString phoneCode)
{
    TGOBJECT(TLType::AuthSignInMethod, method);

    method["phone_number"] = phoneNumber;
    method["phone_code_hash"] = phoneCodeHash;
    method["phone_code"] = phoneCode;

    return sendObject<&writeTLMethodAuthSignIn>(method);
}

TgLongVariant TgClient::authSignUp(QString phoneNumber, QString phoneCodeHash, QString firstName, QString lastName)
{
    TGOBJECT(TLType::AuthSignUpMethod, method);

    method["phone_number"] = phoneNumber;
    method["phone_code_hash"] = phoneCodeHash;
    method["first_name"] = firstName;
    method["last_name"] = lastName;

    return sendObject<&writeTLMethodAuthSignUp>(method);
}

TgLongVariant TgClient::accountGetPassword()
{
    TGOBJECT(TLType::AccountGetPasswordMethod, method);

    return sendObject<&writeTLMethodAccountGetPassword>(method);
}

// Entry point for two-factor authentication. The SRP exchange needs the salts
// and the server's B, and both are single use, so every attempt starts with a
// fresh account.getPassword; the actual check is sent from handleSrpComputed().
TgLongVariant TgClient::authCheckPassword(QString password)
{
    pendingPassword = password;

    return accountGetPassword();
}

TgLongVariant TgClient::authCheckPasswordSRP(TgLongVariant srpId, QByteArray A, QByteArray M1)
{
    TGOBJECT(TLType::AuthCheckPasswordMethod, method);

    TGOBJECT(TLType::InputCheckPasswordSRP, srp);
    srp["srp_id"] = srpId;
    srp["A"] = A;
    srp["M1"] = M1;

    method["password"] = srp;

    return sendObject<&writeTLMethodAuthCheckPassword>(method);
}
